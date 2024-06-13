#pragma once

#include <cstdint>
#include <mutex>

#include "protocols/caracal/include/buffer.hpp"
#include "protocols/common/readwritelock.hpp"
#include "utils/atomic_wrapper.hpp"

template <typename Version_> struct Value {
    using Version = Version_;
    alignas(64) RWLock rwl;
    uint64_t epoch_ = 0;

    Version *master_ = nullptr; // final state

    GlobalVersionArray global_array_; // Global Version Array

    // For contended versions
    RowBuffer *row_buffer_ = nullptr; // Pointer to per-core buffer

    void initialize() { rwl.initialize(); }

    void lock() { rwl.lock(); }

    bool try_lock() { return rwl.try_lock(); }

    void unlock() { rwl.unlock(); }
};
