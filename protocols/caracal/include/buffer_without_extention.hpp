#pragma once

#include <unistd.h>  // for gettid()

#include "protocols/caracal/include/readwriteset.hpp"
#include "protocols/common/readwritelock.hpp"
#include "protocols/ycsb_common/definitions.hpp"
#include "utils/bitmap.hpp"
#include "utils/numa.hpp"

class Version {
 public:
  enum class VersionStatus { PENDING, STABLE };  // status of version

  alignas(64) void* rec;  // nullptr if deleted = true (immutable)
  VersionStatus status;
  bool deleted;  // (immutable)
};


class PerCoreBuffer {  // Caracal's per-core buffer
	public:
  		alignas(64) uint64_t length_ = 0;
  		uint64_t ids_[MAX_SLOTS_OF_PER_CORE_BUFFER] = {0};  // corresponding to slots_
  		Version* slots_[MAX_SLOTS_OF_PER_CORE_BUFFER] = {nullptr};  // To-do: make the size of slot variable &
                 // extend the size of slots

  		void initialize() { length_ = 0; }

  		bool append(Version* version, uint64_t tx_id) {
    		assert(version);

    		// append
    		assert(length_ < MAX_SLOTS_OF_PER_CORE_BUFFER);
    		ids_[length_] = tx_id;
    		slots_[length_] = version;

    		length_++;
    		if (length_ < MAX_SLOTS_OF_PER_CORE_BUFFER)
      			return true;  // the buffer is not full
    		return false;   // the buffer is full
		}
};


// ascending order
class GlobalVersionArray {
 public:
  uint64_t length_ = 0;
  uint64_t ids_[MAX_SLOTS_OF_GLOBAL_ARRAY] = {0};  // corresponding to slots_
  Version* slots_[MAX_SLOTS_OF_GLOBAL_ARRAY] = {
      nullptr};  // To-do: make the size of slot variable &
                 // extend the size of slots

  void append(Version* version, uint64_t tx_id) {
    assert(length_ < MAX_SLOTS_OF_GLOBAL_ARRAY);

    // ascending order
    int i = length_ - 1;
    while (0 <= i && tx_id < ids_[i]) {
      ids_[i + 1] = ids_[i];
      slots_[i + 1] = slots_[i];
      i--;
    }

    ids_[i + 1] = tx_id;
    slots_[i + 1] = version;
    length_++;
  }

  void batch_append(PerCoreBuffer* buffer) {
    for (uint64_t i = 0; i < buffer->length_; i++) {
      append(buffer->slots_[i], buffer->ids_[i]);
    }
    buffer->initialize();
  }

  bool is_exist(uint64_t tx) {
    for (uint64_t i = 0; i < length_; i++) {
      if (ids_[i] == tx) return true;
    }
    return false;
  }

  std::tuple<int, Version*> search_visible_version(uint64_t tx_id) {
    // ascending order
    int i = length_ - 1;
    // 5:  5 20 24 50
    // 60:  50
    while (0 <= i && tx_id <= ids_[i]) {
      i--;
    }

    assert(-1 <= i);
    if (i == -1) return {-1, nullptr};

    assert(ids_[i] < tx_id);
    // tx_id: 4
    // ids_: 1, [3], 4, 5, 6, 10
    assert(slots_[i]);
    return {ids_[i], slots_[i]};
  }

  void initialize() { length_ = 0; }

  bool is_empty() { return length_ == 0; }

  std::tuple<uint64_t, Version*> latest() {
    if (is_empty()) return {0, nullptr};

    return {ids_[length_ - 1], slots_[length_ - 1]};
  }

  void print() {
    for (uint64_t i = 0; i < length_; i++) {
      std::cout << ids_[i] << " ";
    }
    std::cout << std::endl;
  }
};

class RowBuffer {  // Caracal's per-core buffer
 public:
  PerCoreBuffer*
      buffers_[LOGICAL_CORE_SIZE];  // TODO: alignas(64) をつけるか検討

  void initialize(uint64_t core) { buffers_[core]->initialize(); }

  RowBuffer() {
    pid_t main_tid = gettid();  // fetch the main thread's tid
    for (uint64_t core_id = 0; core_id < LOGICAL_CORE_SIZE; core_id++) {
      Numa numa(main_tid,
                core_id);  // move to the designated core from main thread

      buffers_[core_id] = new PerCoreBuffer();
    }
  }
};

class RowBufferController {
 private:
  RowBuffer buffers_[NUM_REGIONS];
  RWLock lock_;
  uint64_t used_ = 0;  // how many buffers currently used

 public:
  RowBuffer* fetch_new_buffer() {
    lock_.lock();
    assert(used_ < NUM_REGIONS);
    RowBuffer* buffer = &buffers_[used_];  // TODO: reconsider
    used_++;
    lock_.unlock();
    return buffer;
  }
};
