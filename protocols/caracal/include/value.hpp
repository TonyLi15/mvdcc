#pragma once

#include <cstdint>
#include <mutex>

#include "protocols/caracal/include/row_buffer.hpp"
#include "utils/atomic_wrapper.hpp"

struct Value {
    alignas(64) uint64_t epoch_ = 0;

    GlobalVersionArray global_array_; // Global Version Array

    // For contended versions
    RowBuffer *row_buffer_ = nullptr; // Pointer to per-core buffer
};
