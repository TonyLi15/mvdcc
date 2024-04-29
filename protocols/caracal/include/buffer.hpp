/*
Apr 26, 2024
use 1 vector of pairs instead of 2 vectors of ids and slots
in per-core buffer and global version array
*/

#pragma once

#include <algorithm> // for find()
#include <unistd.h>  // for gettid()

#include "protocols/caracal/include/readwriteset.hpp"
#include "protocols/common/readwritelock.hpp"
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

class PerCoreBuffer { // Caracal's per-core buffer
  public:
    // use pair data structure
    std::vector<std::pair<uint64_t, Version *>> ids_slots_ = {{0, nullptr}};

    void initialize() { ids_slots_.clear(); }

    bool appendable(Version *version, uint64_t tx_id) {
        assert(version);

        // append to per-core buffer
        assert(ids_slots_.size() < MAX_SLOTS_OF_PER_CORE_BUFFER);
        // Find the position to append the version
        auto it = std::upper_bound(ids_slots_.begin(), ids_slots_.end(),
                                   std::make_pair(tx_id, nullptr),
                                   [](const auto &pair, const auto &value) {
                                       return pair.first < value.first;
                                   });

        // Append to the corresponding position
        ids_slots_.insert(it, std::make_pair(tx_id, version));

        if (ids_slots_.size() < MAX_SLOTS_OF_PER_CORE_BUFFER)
            return true; // the buffer is not full
        return false;    // the buffer is full
    }
};

// ascending order
class GlobalVersionArray {
  public:
    std::vector<std::pair<uint64_t, Version *>> ids_slots_ = {{0, nullptr}};

    void append(Version *version, uint64_t tx_id) {
        assert(ids_slots_.size() < MAX_SLOTS_OF_GLOBAL_ARRAY);

        // Find the position to append the version
        auto it = std::upper_bound(ids_slots_.begin(), ids_slots_.end(),
                                   std::make_pair(tx_id, nullptr),
                                   [](const auto &pair, const auto &value) {
                                       return pair.first < value.first;
                                   });

        // Append to the corresponding position
        ids_slots_.insert(it, std::make_pair(tx_id, version));
    }

    void batch_append(PerCoreBuffer *buffer) {
        for (uint64_t i = 0; i < buffer->ids_slots_.size(); i++) {
            append(buffer->ids_slots_[i].second, buffer->ids_slots_[i].first);
        }
        buffer->initialize();
    }

    bool is_exist(uint64_t tx) {
        // Use std::find with a custom comparator
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

    void initialize() { ids_slots_.clear(); }

    bool is_empty() { return ids_slots_.empty(); }

    std::pair<uint64_t, Version *> latest() {
        if (is_empty())
            return {0, nullptr};

        return ids_slots_.back();
    }
};

class RowBuffer { // Caracal's per-core buffer
  public:
    PerCoreBuffer
        *buffers_[LOGICAL_CORE_SIZE]; // TODO: alignas(64) をつけるか検討

    void initialize(uint64_t core) { buffers_[core]->initialize(); }

    RowBuffer() {
        pid_t main_tid = gettid(); // fetch the main thread's tid
        for (uint64_t core_id = 0; core_id < LOGICAL_CORE_SIZE; core_id++) {
            Numa numa(main_tid,
                      core_id); // move to the designated core from main thread

            buffers_[core_id] = new PerCoreBuffer();
        }
    }
};

class RowBufferController {
  private:
    RowBuffer buffers_[NUM_REGIONS];
    RWLock lock_;
    uint64_t used_ = 0; // how many buffers currently used

  public:
    RowBuffer *fetch_new_buffer() {
        lock_.lock();
        assert(used_ < NUM_REGIONS);
        RowBuffer *buffer = &buffers_[used_]; // TODO: reconsider
        used_++;
        lock_.unlock();
        return buffer;
    }
};
