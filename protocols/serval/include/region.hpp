#pragma once

#include <unistd.h>  // for gettid()

#include "protocols/serval/include/readwriteset.hpp"
#include "protocols/ycsb_common/definitions.hpp"
#include "utils/bitmap.hpp"
#include "utils/numa.hpp"
#include "protocols/common/readwritelock.hpp"

class Version {
 public:
  enum class VersionStatus { PENDING, STABLE };  // status of version

  alignas(64) void* rec;  // nullptr if deleted = true (immutable)
  VersionStatus status;
  bool deleted;  // (immutable)
};

// ascending order
class GlobalVersionArray {
 public:
  uint64_t length_ = 0;
  std::vector<uint64_t> ids_ = {0}; // corresponding to slots_
  std::vector<Version*> slots_ = {nullptr};

  bool is_exist(uint64_t tx) {
    for (uint64_t i = 0; i < length_; i++) {
      if (ids_[i] == tx) return true;
    }
    return false;
  }

  void print() {
    for (uint64_t i = 0; i < length_; i++) {
      std::cout << ids_[i] << " ";
    }
    std::cout << std::endl;
  }

  std::tuple<uint64_t, Version*> search_visible_version(uint64_t tx_id) {
    // ascending order
    int i = length_ - 1;
    // 5:  5 20 24 50
    while (0 <= i && tx_id <= ids_[i]) {
      i--;
    }

    assert(-1 <= i);
    if (i == -1) return {0, nullptr};

    assert(ids_[i] < tx_id);
    // tx_id: 4
    // ids_: 1, [3], 4, 5, 6, 10
    return {ids_[i], slots_[i]};
  }

  void append(Version* version, uint64_t tx_id,
              [[maybe_unused]] uint64_t core) {
    assert(tx_id / 64 == core);
    assert(length_ < MAX_SLOTS_OF_GLOBAL_ARRAY);

    // ascending order
    int i = length_ - 1;
    while (0 <= i && tx_id < ids_[i]) {
      ids_[i + 1] = ids_[i];
      slots_[i + 1] = slots_[i];
      i--;
    }

    ids_.insert(ids_.begin() + i + 1, tx_id);
    slots_.insert(slots_.begin() + i + 1, version);
    length_++;
  }

  void initialize() { length_ = 0; }

  bool is_empty() { return length_ == 0; }

  std::tuple<uint64_t, Version*> latest() {
    if (is_empty()) return {0, nullptr};

    return {ids_[length_ - 1], slots_[length_ - 1]};
  }
};

class PerCoreVersionArray {  // Caracal's per-core buffer
 public:
  alignas(64) uint64_t transaction_bitmap_ = 0;
  std::vector<Version*> slots_ = {nullptr};

  void initialize() { transaction_bitmap_ = 0; }

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

  Version* get(uint64_t tx) {
    assert(tx < 64);
    uint64_t mask = fill_the_left_side_to_the_given_position(tx);
    int len = __builtin_popcountll(transaction_bitmap_ & mask);
    assert(slots_[len - 1]);
    return slots_[len - 1];
  }

  Version* latest() {
    assert(0 < length());
    return slots_[length() - 1];
  }

  void append(Version* version, uint64_t tx_id) {
    assert(version);
    assert(tx_id < 64);

    // append
    assert(length() < MAX_SLOTS_OF_PER_CORE_ARRAY);
    int len = length();  // 0 - 64
    // len:          0 1 2 3 ... 64
    // append index: 0 1 2 3 ... x
    
    //slots_[len] = version;
    slots_.insert(slots_.begin() + len, version);

    // update transaction_bitmap_
    update_transaction_bitmap(tx_id);

    assert(is_bit_set_at_the_position(transaction_bitmap_, tx_id));
    assert(len + 1 == length());
  }
};

class RowRegion {  // Serval's RowRegion
 public:
  PerCoreVersionArray*
      arrays_[LOGICAL_CORE_SIZE];  // TODO: alignas(64) をつけるか検討

  void initialize(uint64_t core) { arrays_[core]->initialize(); }

  void append(uint64_t core, Version* version, uint64_t tx) {
    assert(version);
    assert(tx < 64);
    arrays_[core]->append(version, tx);
  }

  RowRegion() {
    pid_t main_tid = gettid();  // fetch the main thread's tid
    for (uint64_t core_id = 0; core_id < LOGICAL_CORE_SIZE; core_id++) {
      Numa numa(main_tid,
                core_id);  // move to the designated core from main thread

      arrays_[core_id] = new PerCoreVersionArray();
    }
  }
};

class RowRegionController {
 private:
  RowRegion regions_[NUM_REGIONS];
  RWLock lock_;
  uint64_t used_ = 0;  // how many regions currently used

 public:
  RowRegion* fetch_new_region() {
    lock_.lock();
    assert(used_ < NUM_REGIONS);
    RowRegion* region = &regions_[used_];  // TODO: reconsider
    used_++;
    lock_.unlock();
    return region;
  }
};
