#pragma once

#include <cstdint>
#include <mutex>

#include "protocols/common/readwritelock.hpp"
#include "protocols/serval/include/region.hpp"
#include "utils/atomic_wrapper.hpp"

template <typename Version_>
struct Value {
  using Version = Version_;
  alignas(64) RWLock rwl;
  uint64_t epoch_ = 0;

  Version* master_ = nullptr;  // final state

  GlobalVersionArray global_array_;  // Global Version Array

  // For contended versions
  RowRegion* row_region_ = nullptr;  // Pointer to per-core version array
  uint64_t core_bitmap_ = 0;

  void initialize() { rwl.initialize(); }

  void lock() { rwl.lock(); }

  bool try_lock() { return rwl.try_lock(); }

  void unlock() { rwl.unlock(); }
};
