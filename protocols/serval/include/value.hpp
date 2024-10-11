#pragma once

#include <cstdint>
#include <mutex>

#include "protocols/common/readwritelock.hpp"
#include "protocols/serval/include/row_region.hpp"
#include "utils/atomic_wrapper.hpp"

struct Value {
    alignas(64) RWLock rwl;
    uint64_t epoch_ = 0;

    Version *master_ = nullptr; // final state

    GlobalVersionArray global_array_; // Global Version Array

    // For contended versions
    RowRegion *row_region_ = nullptr; // Pointer to per-core version array

    bool has_dirty_region() {
        if (__atomic_load_n(&row_region_, __ATOMIC_SEQ_CST)) { // TODO: 再考
            return row_region_->is_dirty();
        }
        return false;
    }

    void initialize() { rwl.initialize(); }

    void lock() { rwl.lock(); }

    bool try_lock() { return rwl.try_lock(); }

    void unlock() { rwl.unlock(); }

    void gc_master_version(Version *latest, Stat &stat) {
        assert(master_);
        assert(master_->rec);
        delete reinterpret_cast<Record *>(master_->rec);
        delete master_;
        stat.increment(Stat::MeasureType::Delete);
        master_ = latest;
    }

    void initialize_the_row(uint64_t epoch, Stat &stat) {
        /*
        [!global_array_.is_dirty() && !has_dirty_region()]
        epoch 7: major gc (initialize_the_row)
        epoch 10: initialization phase (initialize_the_row)
        */
        assert(!(global_array_.is_dirty() && has_dirty_region()));

        // 1. store the final state of one previous epoch to val->master_
        if (global_array_.is_dirty()) {
            assert(!has_dirty_region());
            auto [id, latest] = global_array_.pop_final_state();
            assert(latest);
            global_array_.gc(stat);
            gc_master_version(latest, stat);
        } else if (has_dirty_region()) {
            assert(!global_array_.is_dirty());
            auto [id, latest] = row_region_->pop_final_state();
            assert(latest);
            row_region_->initialize_core_bitmap(); // initialize
            gc_master_version(latest, stat);
        }

        // 2. update val->epoch_
        asm volatile("" : : : "memory");
        __atomic_store_n(&epoch_, epoch, __ATOMIC_SEQ_CST);
        asm volatile("" : : : "memory");
    }
};
