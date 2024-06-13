#pragma once

#include <cassert>
#include <cstring>
#include <set>
#include <stdexcept>

#include "indexes/masstree.hpp"
#include "protocols/caracal/include/buffer.hpp"
#include "protocols/caracal/include/readwriteset.hpp"
#include "protocols/caracal/include/value.hpp"
// #include "protocols/common/timestamp_manager.hpp"
#include "protocols/common/transaction_id.hpp"
#include "protocols/ycsb_common/definitions.hpp" // for delete Record
#include "utils/bitmap.hpp"
#include "utils/logger.hpp"
#include "utils/tsc.hpp"
#include "utils/utils.hpp"

template <typename Index> class Caracal {
  public:
    using Key = typename Index::Key;
    using Value = typename Index::Value;
    using Version = typename Value::Version;
    using LeafNode = typename Index::LeafNode;
    using NodeInfo = typename Index::NodeInfo;

    Caracal(uint64_t core_id, uint64_t txid, RowBufferController &rrc,
            Stat &stat)
        : core_(core_id), txid_(txid), rrc_(rrc), stat_(stat) {}

    ~Caracal() {}

    void terminate_transaction() {
        for (TableID table_id : tables) {
            auto &w_table = ws.get_table(table_id);
            w_table.clear();
        }
        tables.clear();
    }

    void append_pending_version(TableID table_id, Key key, Version *&pending) {
        Index &idx = Index::get_index();

        tables.insert(table_id);
        std::vector<Key> &w_table = ws.get_table(table_id);
        typename std::vector<Key>::iterator w_iter =
            std::find(w_table.begin(), w_table.end(), key);

        // Case of not append occur
        if (w_iter == w_table.end()) {
            Value *val;
            typename Index::Result res = idx.find(
                table_id, key, val); // find corresponding index in masstree

            if (res == Index::Result::NOT_FOUND) {
                assert(false);
                throw std::runtime_error(
                    "masstree NOT_FOUND"); // TODO: この場合、どうするかを考える
                return;
            }

            // Got value from masstree

            do_append_pending_version(val, pending);
            assert(pending);

            // Place it in writeset
            auto &w_table = ws.get_table(table_id);
            w_table.emplace_back(key);
        }

        // TODO: Case of found in read or written set
    }

    const Rec *read(TableID table_id, Key key) {
        Index &idx = Index::get_index();

        Value *val;
        typename Index::Result res = idx.find(
            table_id, key, val); // find corresponding index in masstree

        if (res == Index::Result::NOT_FOUND) {
            assert(false);
            throw std::runtime_error(
                "masstree NOT_FOUND"); // TODO: この場合、どうするかを考える
            return nullptr;
        }

        Version *visible = nullptr;

        // any append is not executed in the row in the current epoch
        // and read the final state in one previous epoch
        uint64_t epoch = val->epoch_;
        if (epoch != epoch_) {
            visible = val->master_;
            return execute_read(visible);
        }

        // append is executed in the current epoch
        assert(epoch == epoch_);
        auto [txid_in_global_array, visible_in_global_array] =
            val->global_array_.search_visible_version(txid_);
        assert(txid_in_global_array < (int)txid_);

        if (visible_in_global_array) {
            assert(0 <= txid_in_global_array &&
                   txid_in_global_array < static_cast<int>(txid_));
            assert(txid_in_global_array != static_cast<int>(txid_));
            visible = visible_in_global_array;
            return wait_stable_and_execute_read(visible);
        } else {
            visible = val->master_;
            return execute_read(visible);
        }

        assert(false);
        return nullptr;
    }

    Rec *write(TableID table_id, Version *pending) {
        return upsert(table_id, pending);
    }

    Rec *upsert(TableID table_id, Version *pending) {
        const Schema &sch = Schema::get_schema();
        size_t record_size = sch.get_record_size(table_id);

        // Rec *rec = MemoryAllocator::aligned_allocate(record_size);
        Rec *rec = reinterpret_cast<Rec *>(operator new(record_size));

        __atomic_store_n(&pending->rec, rec, __ATOMIC_SEQ_CST); // write
        __atomic_store_n(&pending->status, Version::VersionStatus::STABLE,
                         __ATOMIC_SEQ_CST);
        return rec;
    }

    // called in end of the initialization phase
    // void finalize_batch_append() {
    //     uint64_t start = rdtscp();
    //     while (!appended_core_buffers_.empty()) {
    //         auto itr = appended_core_buffers_.begin();
    //         Value *val = itr->first;
    //         PerCoreBuffer *buffer = itr->second;
    //         // bufferに残っているpendingsをバッチアペンドする
    //         if (!val->try_lock())
    //             continue;
    //         val->global_array_.batch_append(buffer);
    //         val->unlock();
    //         appended_core_buffers_.erase(itr);
    //     }
    //     stat_.add(Stat::MeasureType::FinalizeInitializationTime,
    //               rdtscp() - start);
    // }

    void finalize_batch_append() {
        while (!appended_core_buffers_.empty()) {
            auto itr = appended_core_buffers_.begin();
            Value *val = itr->first;
            PerCoreBuffer *buffer = itr->second;
            // bufferに残っているpendingsをバッチアペンドする
            uint64_t start = rdtscp();
            while (!val->try_lock()) {
                // spin
            }
            stat_.add(Stat::MeasureType::WaitInInitialization,
                      rdtscp() - start);
            val->global_array_.batch_append(buffer);
            val->unlock();
            appended_core_buffers_.erase(itr);
        }
    }

    uint64_t core_;
    uint64_t txid_;
    uint64_t epoch_ = 0;

  private:
    WriteSet<Key> ws; // write set
    std::set<TableID> tables;

    RowBuffer *spare_buffer_ = nullptr;

    RowBufferController &rrc_;

    Stat &stat_;

    std::unordered_map<Value *, PerCoreBuffer *> appended_core_buffers_;

    void do_append_pending_version(Value *val, Version *&pending) {
        assert(!pending);

        // all threads wait until the first thread to arrive at the val
        // initialize the val and update the val->epoch_
        epoch_guard(val, pending);
        if (pending)
            return;

        // val->epoch_ == epoch_
        RowBuffer *cur_buffer =
            __atomic_load_n(&val->row_buffer_, __ATOMIC_SEQ_CST);

        // the val will or may be contended if the row_buffer_ is already exist.
        if (may_be_contented(cur_buffer)) {
            pending = append_to_contented_row(val, cur_buffer->buffers_[core_]);
            return;
        }

        // the val may be uncontended
        if (val->try_lock()) {
            pending = append_to_uncontended_row(val);
            return;
        }

        // the thread couldn't acquire the lock: the val is getting crowded.
        RowBuffer *new_buffer;

        if (spare_buffer_) {
            new_buffer = spare_buffer_;
            spare_buffer_ = nullptr;
        } else {
            new_buffer = rrc_.fetch_new_buffer();
        }

        assert(cur_buffer == nullptr);
        if (try_install_new_buffer(val, cur_buffer, new_buffer)) {
            pending = append_to_contented_row(val, new_buffer->buffers_[core_]);
            return;
        }

        // other thread already install new buffer　
        RowBuffer *other_buffer =
            __atomic_load_n(&val->row_buffer_, __ATOMIC_SEQ_CST);
        spare_buffer_ = new_buffer;
        pending = append_to_contented_row(val, other_buffer->buffers_[core_]);
        return;
    }

    void epoch_guard(Value *val, Version *&pending) {
        uint64_t epoch;

        while ((epoch = __atomic_load_n(&val->epoch_, __ATOMIC_SEQ_CST)) <
               epoch_) {
            /*
             case1: val->epoch_ < epoch_
             */
            assert(epoch < epoch_);

            if (!val->try_lock())
                continue;

            assert(val->epoch_ <= epoch_);
            // 再確認
            if (val->epoch_ < epoch_) { // 未初期化
                // the first transaction in the current epoch to arrive on the
                // val should initialize the val
                pending = initialize_the_row_and_append(
                    val); // call unlock inside the function
            } else if (val->epoch_ == epoch_) { // 　初期化ずみ
                pending = append_to_uncontended_row(
                    val); // call unlock inside the function
            } else {
                assert(false);
            }

            return;
        };
    }

    void minor_gc_master_version(Version *&master) {
        assert(master);
        assert(master->rec);
        delete reinterpret_cast<Record *>(master->rec);
        delete master;
        // MemoryAllocator::deallocate(master->rec);
        // MemoryAllocator::deallocate(master);
    }

    Version *initialize_the_row_and_append(Value *val) {
        minor_gc_master_version(val->master_);

        // 1. store the final state of one previous epoch to val->master_
        assert(val->epoch_ < epoch_);
        val->master_ = val->global_array_.find_final_state_and_initialize();
        assert(val->master_);

        // 3. append to the global_array_
        Version *pending = create_pending_version();
        val->global_array_.append(pending, txid_);

        // 4. update val->epoch_
        val->epoch_ = epoch_;
        assert(val->global_array_.is_exist(txid_));

        val->unlock();

        return pending;
    }

    bool try_install_new_buffer(Value *val, RowBuffer *cur_buffer,
                                RowBuffer *new_buffer) {
        assert(cur_buffer == nullptr);
        return __atomic_compare_exchange_n(&val->row_buffer_, &cur_buffer,
                                           new_buffer, false, __ATOMIC_SEQ_CST,
                                           __ATOMIC_SEQ_CST);
    }

    // lock should be acquired before this function is called
    Version *append_to_uncontended_row(Value *val) {
        Version *pending = create_pending_version();

        // Append pending version to global version chain
        val->global_array_.append(pending, txid_);
        assert(val->global_array_.is_exist(txid_));

        val->unlock();

        return pending;
    }

    bool may_be_contented(RowBuffer *row_buffer) {
        return row_buffer != nullptr;
    }

    Version *append_to_contented_row(Value *val, PerCoreBuffer *core_buffer) {
        Version *pending = create_pending_version();

        if (core_buffer->appendable(pending, txid_)) {
            // not full
            appended_core_buffers_.insert({val, core_buffer}); // TODO: 高速化
        } else {
            // per core buffer is full and append to global array
            uint64_t start = rdtscp();
            while (!val->try_lock()) {
                // spin
            };
            stat_.add(Stat::MeasureType::WaitInInitialization,
                      rdtscp() - start);
            val->global_array_.batch_append(core_buffer);
            val->unlock();
        };

        return pending;
    }

    Version *create_pending_version() {
        // Version *version = reinterpret_cast<Version *>(
        //     MemoryAllocator::aligned_allocate(sizeof(Version)));
        Version *version = new Version;
        assert(version);
        version->rec = nullptr; // TODO
        version->deleted = false;
        version->status = Version::VersionStatus::PENDING;
        return version;
    }

    Rec *execute_read(Version *visible) {
        assert(visible);
        return visible->rec;
    }

    Rec *wait_stable_and_execute_read(Version *visible) {
        assert(visible);
        uint64_t start = rdtscp();
        while (__atomic_load_n(&visible->status, __ATOMIC_SEQ_CST) ==
               Version::VersionStatus::PENDING) {
            // spin
            asm volatile("pause" : : : "memory"); // equivalent to "rep; nop"
        }                                         // TODO: やばい
        stat_.add(Stat::MeasureType::WaitInExecution, rdtscp() - start);
        return execute_read(visible);
    }
};