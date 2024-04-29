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

    alignas(64) void *rec; // nullptr if deleted = true (immutable)
    VersionStatus status;
    bool deleted; // (immutable)
};

// sort versions with ascending order
class GlobalVersionArray {
  public:
    std::vector<std::pair<uint64_t, Version *>> ids_slots_ = {{0, nullptr}};

    bool is_exist(uint64_t tx) {
        auto it =
            std::find_if(ids_slots_.begin(), ids_slots_.end(),
                         [tx](const auto &pair) { return pair.first == tx; });

        return it != ids_slots_.end();
    }

    std::pair<uint64_t, Version *> search_visible_version(uint64_t tx_id) {
        // Handle the case where tx_id is smaller than or equal to the smallest
        // tx_id in ids_slots_
        if (ids_slots_.empty() || tx_id <= ids_slots_.begin()->first) {
            return {0, nullptr};
        }

        // Find the first element whose tx_id is greater than the given tx_id
        auto it = std::upper_bound(ids_slots_.begin(), ids_slots_.end(), tx_id,
                                   [](uint64_t tx_id, const auto &pair) {
                                       return tx_id < pair.first;
                                   });

        // Move the iterator back to the previous element
        --it;

        // Return the tx_id and the corresponding Version* object
        return {it->first, it->second};
    }

    void append(Version *version, uint64_t tx_id,
                [[maybe_unused]] uint64_t core) {
        assert(tx_id / 64 == core);
        assert(ids_slots_.size() < MAX_SLOTS_OF_GLOBAL_ARRAY);

        // Find the position to insert the new element
        auto it = std::upper_bound(ids_slots_.begin(), ids_slots_.end(),
                                   std::make_pair(tx_id, nullptr),
                                   [](const auto &pair, const auto &value) {
                                       return pair.first < value.first;
                                   });

        // Insert the new element at the found position
        ids_slots_.insert(it, std::make_pair(tx_id, version));
    }

    void initialize() { ids_slots_.clear(); }

    bool is_empty() { return ids_slots_.empty(); }

    std::pair<uint64_t, Version *> latest() {
        if (is_empty())
            return {0, nullptr};

        return ids_slots_.back();
    }
};

class PerCoreVersionArray { // Serval's per-core version array
  public:
    alignas(64) uint64_t transaction_bitmap_ = 0;
    std::vector<Version *> slots_ = {nullptr};

    void initialize() {
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
        uint64_t mask = fill_the_left_side_to_the_given_position(tx);
        int len = __builtin_popcountll(transaction_bitmap_ & mask);
        assert(slots_[len - 1]);
        return slots_[len - 1];
    }

    Version *latest() {
        assert(0 < length());
        return slots_[length() - 1];
    }

    void append(Version *version, uint64_t tx_id) {
        assert(version);
        assert(tx_id < 64);

        // append
        assert(length() < MAX_SLOTS_OF_PER_CORE_ARRAY);
        int len = length(); // 0 - 64
        // len:          0 1 2 3 ... 64
        // append index: 0 1 2 3 ... x

        // slots_[len] = version;
        slots_.insert(slots_.begin() + len, version);

        // update transaction_bitmap_
        update_transaction_bitmap(tx_id);

        assert(is_bit_set_at_the_position(transaction_bitmap_, tx_id));
        assert(len + 1 == length());
    }
};

class RowRegion { // Serval's Row Region
  public:
    PerCoreVersionArray
        *arrays_[LOGICAL_CORE_SIZE]; // TODO: alignas(64) をつけるか検討

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
            arrays_[core_id] = new PerCoreVersionArray();
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
        RowRegion *region = &regions_[used_]; // TODO: reconsider
        used_++;
        lock_.unlock();
        return region;
    }
};
