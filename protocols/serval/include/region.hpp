#pragma once

#include <unistd.h> // for gettid()

#include <algorithm> // for find()

#include "protocols/common/readwritelock.hpp"
#include "protocols/serval/include/readwriteset.hpp"
#include "protocols/ycsb_common/definitions.hpp"
#include "utils/bitmap.hpp"
#include "utils/numa.hpp"

class Version {
  public:
    enum class VersionStatus { PENDING, STABLE }; // status of version

    alignas(64) void *rec = nullptr; // nullptr if deleted = true (immutable)
    VersionStatus status;
    bool deleted; // (immutable)
};

// ascending order
class GlobalVersionArray {
  public:
    std::vector<std::pair<int, Version *>> ids_slots_;

    bool is_exist(int tx) {
        // Use std::find with a custom comparator
        auto it =
            std::find_if(ids_slots_.begin(), ids_slots_.end(),
                         [tx](const auto &pair) { return pair.first == tx; });
        return it != ids_slots_.end();
    }

    std::pair<int, Version *> search_visible_version(int tx_id) {
        // Handle the case where tx_id is smaller than or equal to the smallest
        // tx_id in ids_slots_
        if (ids_slots_.empty() || tx_id <= ids_slots_.begin()->first) {
            return {-1, nullptr};
        }

        // Find the first element whose tx_id is greater than the given tx_id
        auto it = std::upper_bound(
            ids_slots_.begin(), ids_slots_.end(), tx_id,
            [](int tx_id, const auto &pair) { return tx_id <= pair.first; });

        // Move the iterator back to the previous element
        --it;

        // Return the tx_id and the corresponding Version* object
        return {it->first, it->second};
    }

    void append(Version *version, int tx_id) {
        assert(version);

        // Find the position to insert the new element
        auto it = std::upper_bound(ids_slots_.begin(), ids_slots_.end(),
                                   std::make_pair(tx_id, nullptr),
                                   [](const auto &pair, const auto &value) {
                                       return pair.first < value.first;
                                   });

        // Insert the new element at the found position
        ids_slots_.insert(it, std::make_pair(tx_id, version));
    }

    void minor_gc() {
        for (auto &[_, version] : ids_slots_) {
            assert(version);
            assert(version->status == Version::VersionStatus::STABLE);
            assert(version->rec);
            // MemoryAllocator::deallocate(version->rec);
            // MemoryAllocator::deallocate(version);
            delete reinterpret_cast<Record *>(version->rec);
            delete version;
            version = nullptr;
        }
    }

    std::pair<int, Version *> find_final_state_and_initialize() {
        if (is_empty()) {
            return {-1, nullptr};
        }

        auto last = ids_slots_.back(); // 最後尾を取得
        ids_slots_.pop_back();         // 最後尾を削除

        minor_gc();
        ids_slots_.clear();

        return last;
    }

    bool is_empty() { return ids_slots_.empty(); }

    std::pair<int, Version *> latest() {
        if (is_empty())
            return {-1, nullptr};

        return ids_slots_.back();
    }
};

class PerCoreVersionArray { // Serval's per-core version array
  public:
    alignas(64) uint64_t transaction_bitmap_ = 0;
    std::vector<Version *> slots_;

    void minor_gc() {
        for (Version *&version : slots_) {
            assert(version);
            assert(version->status == Version::VersionStatus::STABLE);
            assert(version->rec);
            delete reinterpret_cast<Record *>(version->rec);
            delete version;
            version = nullptr;
        }
    }

    void initialize() {
        minor_gc();
        transaction_bitmap_ = 0;
        slots_.clear();
    }

    void update_transaction_bitmap(uint64_t tx_id) {
        assert(!is_bit_set_at_the_position(transaction_bitmap_, tx_id));

        // change transaction bitmap's corresponding bit to 1
        [[maybe_unused]] uint64_t bits = transaction_bitmap_;
        transaction_bitmap_ =
            set_bit_at_the_given_location(transaction_bitmap_, tx_id);
        assert(bits != transaction_bitmap_);
        assert(__builtin_popcountll(bits) + 1 ==
               __builtin_popcountll(transaction_bitmap_));
        assert(is_bit_set_at_the_position(transaction_bitmap_, tx_id));
    }

    int length() { return __builtin_popcountll(transaction_bitmap_); }

    Version *get(uint64_t tx) {
        assert(tx < 64);
        assert(set_bit_at_the_given_location(tx));
        uint64_t mask = fill_the_left_side_until_the_given_position(tx);
        /*
        tx: 3

        len: 1
        index: 0
        0001 0100 1000 ... 0001
        slots: tx3, tx5, ...

        len: 3
        index: 2
        1101 0100 1000 ... 0001
        slots: tx0, tx1, [tx3], tx5, ...

        len: 4
        index: 3
        1111 0100 1000 ... 0001
        slots: tx0, tx1, tx2, [tx3], tx5, ...
        */
        int len = __builtin_popcountll(transaction_bitmap_ & mask);
        int index = len - 1;
        assert(0 <= index);
        assert(slots_[index]);
        return slots_[index];
    }

    Version *latest() {
        assert(0 < length());
        assert(length() == (int)slots_.size());
        return slots_[length() - 1];
    }

    std::pair<int, Version *> get_latest() {
        assert(0 < length());
        assert(length() == (int)slots_.size());

        Version *last = slots_.back();
        slots_.pop_back(); // 最後尾を削除

        return {find_the_largest(transaction_bitmap_), last};
    }

    void append(Version *version, uint64_t tx_id) {
        assert(version);
        assert(tx_id < 64);

        // append
        assert(length() < MAX_SLOTS_OF_PER_CORE_ARRAY);
        int len = length(); // 0 - 64
                            // len:          0 1 2 3 ... 64
                            // append index: 0 1 2 3 ... x
        assert(len == (int)slots_.size());
        assert(slots_.size() < 64);
        // slots_[len] = version;
        slots_.insert(slots_.begin() + len, version);
        assert(slots_.size() < 64);

        // update transaction_bitmap_
        update_transaction_bitmap(tx_id);

        assert(length() == (int)slots_.size());
        assert(is_bit_set_at_the_position(transaction_bitmap_, tx_id));
        assert(len + 1 == length());
    }
};

class RowRegion { // Serval's RowRegion
  public:
    PerCoreVersionArray
        *arrays_[LOGICAL_CORE_SIZE]; // TODO: alignas(64) をつけるか検討

    std::pair<int, Version *> find_final_state(uint64_t core) {
        return arrays_[core]->get_latest();
    }

    void initialize(uint64_t core) { arrays_[core]->initialize(); }

    void append(uint64_t core, Version *version, uint64_t tx) {
        assert(version);
        assert(tx < 64);
        arrays_[core]->append(version, tx);
    }

    RowRegion() {
        pid_t tid = gettid(); // fetch the thread's tid
        for (uint64_t core_id = 0; core_id < LOGICAL_CORE_SIZE; core_id++) {
            Numa numa(tid, core_id); // move to the designated core
                                     // arrays_[core_id] =
            //     new (MemoryAllocator::allocate(sizeof(PerCoreVersionArray)))
            //         PerCoreVersionArray(); // TOOD: あってる？
            arrays_[core_id] = new PerCoreVersionArray; // TOOD: あってる？
        }
    }

    ~RowRegion() {
        for (uint64_t core_id = 0; core_id < LOGICAL_CORE_SIZE; core_id++) {
            // MemoryAllocator::deallocate(arrays_[core_id]);
            delete arrays_[core_id];
        }
    }
};

class RowRegionController {
  private:
    RowRegion regions_[NUM_REGIONS];
    RWLock lock_;
    uint64_t used_ = 0; // how many regions currently used

  public:
    RowRegion *fetch_new_region() {
        lock_.lock();
        assert(used_ < NUM_REGIONS);
        if (NUM_REGIONS <= used_) {
            throw std::runtime_error("NUM_REGIONS <= used_");
        }
        RowRegion *region = &regions_[used_]; // TODO: reconsider
        used_++;
        lock_.unlock();
        return region;
    }
};
