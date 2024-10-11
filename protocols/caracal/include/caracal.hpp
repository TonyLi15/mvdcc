#pragma once

#include <cassert>
#include <cstring>
#include <set>
#include <stdexcept>

#include "indexes/masstree.hpp"
#include "protocols/caracal/include/major_gc.hpp"
#include "protocols/caracal/include/readwriteset.hpp"
#include "protocols/caracal/include/row_buffer.hpp"
#include "protocols/caracal/include/value.hpp"
#include "protocols/caracal/include/version.hpp"

// #include "protocols/common/timestamp_manager.hpp"
#include "protocols/common/transaction_id.hpp"
#include "protocols/ycsb_common/definitions.hpp"  // for delete Record
#include "utils/bitmap.hpp"
#include "utils/logger.hpp"
#include "utils/tsc.hpp"
#include "utils/utils.hpp"

template <typename Index>
class Caracal {
 public:
  using Key = typename Index::Key;
  using Value = typename Index::Value;
  using LeafNode = typename Index::LeafNode;
  using NodeInfo = typename Index::NodeInfo;

  Caracal(uint64_t core_id, uint64_t txid, RowBufferController &rrc, Stat &stat,
          MajorGC &gc)
      : core_(core_id),
        serial_id_(txid),
        rrc_(rrc),
        stat_(stat),
        major_gc_(gc) {}

  ~Caracal() {}

  void terminate_transaction() {
    for (TableID table_id : tables) {
      auto &w_table = ws.get_table(table_id);
      w_table.clear();
    }
    tables.clear();
  }

  void append_pending_version(TableID table_id, Key key, Version *&pending) {
    assert(0 < epoch_);
    Index &idx = Index::get_index();

    tables.insert(table_id);
    std::vector<Key> &w_table = ws.get_table(table_id);
    typename std::vector<Key>::iterator w_iter =
        std::find(w_table.begin(), w_table.end(), key);

    // Case of not append occur
    if (w_iter == w_table.end()) {
      Value *val;
      typename Index::Result res =
          idx.find(table_id, key, val);  // find corresponding index in masstree

      if (res == Index::Result::NOT_FOUND) {
        assert(false);
        throw std::runtime_error(
            "masstree NOT_FOUND");  // TODO: この場合、どうするかを考える
        return;
      }

      // Got value from masstree

      do_append_pending_version(val, pending);
      assert(pending);

      // Place it in writeset
      auto &w_table = ws.get_table(table_id);
      w_table.emplace_back(key);
      return;
    }
    throw std::runtime_error("error");  // TODO: この場合、どうするかを考える

    // TODO: Case of found in read or written set
  }

  const Rec *read(TableID table_id, Key key) {
    Index &idx = Index::get_index();

    Value *val;
    typename Index::Result res =
        idx.find(table_id, key, val);  // find corresponding index in masstree

    if (res == Index::Result::NOT_FOUND) {
      assert(false);
      throw std::runtime_error(
          "masstree NOT_FOUND");  // TODO: この場合、どうするかを考える
      return nullptr;
    }

    auto [visible_id, visible] =
        val->global_array_.search_visible_version(epoch_, serial_id_);
    assert(visible);
    Rec *rec = wait_stable_and_execute_read(visible);

    return rec;
  }

  Rec *write(TableID table_id, Version *pending) {
    return upsert(table_id, pending);
  }

  Rec *upsert(TableID table_id, Version *pending) {
    const Schema &sch = Schema::get_schema();
    size_t record_size = sch.get_record_size(table_id);

    // Rec *rec = MemoryAllocator::aligned_allocate(record_size);
    Rec *rec = reinterpret_cast<Rec *>(operator new(record_size));
    __atomic_store_n(&pending->rec, rec, __ATOMIC_SEQ_CST);  // write
    __atomic_store_n(&pending->status, Version::VersionStatus::STABLE,
                     __ATOMIC_SEQ_CST);

    return rec;
  }

  /*
    At the end of the initialization phase, each core batch-appends all
    versions in its own slice of each row buffer to the version arrays of
    the corresponding rows. A core’s batch-append operations can occur
    while other cores are still run-ning the initialization phase and
    possibly allocating new row buffers.

     If all cores finish initialization at exactly the same time, they may
     contend on some rows during batch-append. If this happens, Caracal
     will delay batch-appending the contending row and process other rows
     first.
  */
  void finalize_batch_append_optimized() {
    while (!appended_core_buffers_.empty()) {
      auto itr = appended_core_buffers_.begin();
      while (itr != appended_core_buffers_.end()) {
        Value *val = itr->first;
        PerCoreBuffer *buffer = itr->second;
        uint64_t start = rdtscp();
        if (!val->global_array_.try_lock()) {
          itr = std::next(itr);
          stat_.add(Stat::MeasureType::WaitInInitialization, rdtscp() - start);
          continue;
        }
        val->global_array_.batch_append(buffer, epoch_, stat_);
        val->global_array_.unlock();
        itr = appended_core_buffers_.erase(itr);
      }
    }
  }

  void finalize_batch_append_non_optimized() {
    while (!appended_core_buffers_.empty()) {
      auto itr = appended_core_buffers_.begin();
      Value *val = itr->first;
      PerCoreBuffer *buffer = itr->second;
      // bufferに残っているpendingsをバッチアペンドする
      val->global_array_.lock();
      val->global_array_.batch_append(buffer, epoch_, stat_);
      val->global_array_.unlock();
      appended_core_buffers_.erase(itr);
    }
  }

  uint64_t core_;
  uint64_t serial_id_;
  uint64_t epoch_;

 private:
  WriteSet<Key> ws;  // write set
  std::set<TableID> tables;

  RowBuffer *spare_buffer_ = nullptr;

  RowBufferController &rrc_;

  Stat &stat_;

  MajorGC &major_gc_;

  std::unordered_map<Value *, PerCoreBuffer *> appended_core_buffers_;

  void do_append_pending_version(Value *val, Version *&pending) {
    assert(!pending);
    RowBuffer *cur_buffer =
        __atomic_load_n(&val->row_buffer_, __ATOMIC_SEQ_CST);

    // the val will or may be contended if the row_buffer_ is already exist.
    if (may_be_contented(cur_buffer)) {
      pending = append_to_contented_row(val, cur_buffer->buffers_[core_]);
      return;
    }

    // the val may be uncontended
    if (val->global_array_.try_lock()) {
      pending = append_to_uncontended_row(val);
      return;
    }

    // couldn't acquire the lock: the val is getting crowded.
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

  bool try_install_new_buffer(Value *val, RowBuffer *cur_buffer,
                              RowBuffer *new_buffer) {
    assert(cur_buffer == nullptr);
    return __atomic_compare_exchange_n(&val->row_buffer_, &cur_buffer,
                                       new_buffer, false, __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST);
  }

  // lock should be acquired before this function is called
  Version *append_to_uncontended_row(Value *val) {
    Version *pending = create_pending_version(val->global_array_);

    // Append pending version to global version chain
    val->global_array_.append_with_gc(epoch_, serial_id_, pending, stat_);
    assert(val->global_array_.is_exist(epoch_, serial_id_));

    val->global_array_.unlock();

    return pending;
  }

  bool may_be_contented(RowBuffer *row_buffer) { return row_buffer != nullptr; }

  Version *append_to_contented_row(Value *val, PerCoreBuffer *core_buffer) {
    Version *pending = create_pending_version(val->global_array_);

    if (core_buffer->appendable(pending, epoch_, serial_id_)) {
      // not full
      appended_core_buffers_.insert({val, core_buffer});  // TODO: 高速化
    } else {
      // per core buffer is full and append to global array
      uint64_t start = rdtscp();
      val->global_array_.lock();
      stat_.add(Stat::MeasureType::WaitInInitialization, rdtscp() - start);
      val->global_array_.batch_append(core_buffer, epoch_, stat_);
      val->global_array_.unlock();
    };

    return pending;
  }

  Version *create_pending_version([[maybe_unused]] GlobalVersionArray &array) {
    Version *version = new Version;
    assert(version);
    version->rec = nullptr;  // TODO
    version->deleted = false;
    version->status = Version::VersionStatus::PENDING;

    stat_.increment(Stat::MeasureType::Create);

    // major_gc_.collect(epoch_, &array);
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
      asm volatile("pause" : : : "memory");  // equivalent to "rep; nop"
    }  // TODO: やばい
    stat_.add(Stat::MeasureType::WaitInExecution, rdtscp() - start);
    return execute_read(visible);
  }
};