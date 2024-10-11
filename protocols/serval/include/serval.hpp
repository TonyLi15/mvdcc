#pragma once

#include <cassert>
#include <cstring>
#include <set>
#include <stdexcept>
#include <unordered_set>

#include "indexes/masstree.hpp"
#include "protocols/common/readwritelock.hpp"
#include "protocols/common/transaction_id.hpp"
#include "protocols/serval/include/major_gc.hpp"
#include "protocols/serval/include/readwriteset.hpp"
#include "protocols/serval/include/row_region.hpp"
#include "protocols/serval/include/value.hpp"
#include "utils/bitmap.hpp"
#include "utils/logger.hpp"
#include "utils/tsc.hpp"
#include "utils/utils.hpp"

template <typename Index>
class Serval {
 public:
  using Key = typename Index::Key;
  using Value = typename Index::Value;
  using LeafNode = typename Index::LeafNode;
  using NodeInfo = typename Index::NodeInfo;

  Serval(uint64_t core_id, uint64_t txid, RowRegionController &rrc, Stat &stat,
         MajorGC &gc)
      : core_(core_id),
        serial_id_(txid),
        rrc_(rrc),
        stat_(stat),
        major_gc_(gc) {}

  ~Serval() {}

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

    Version *visible = nullptr;

    // any append is not executed in the row in the current epoch
    // and read the final state in one previous epoch
    uint64_t epoch = val->epoch_;
    if (epoch != epoch_) {
      visible = val->master_;
      return execute_read(visible);
    }

    assert(epoch == epoch_);

    if (val->global_array_.is_dirty()) {
      auto [is_found, txid, v] =
          val->global_array_.search_visible_version(serial_id_);

      if (is_found) {
        assert((uint64_t)txid < serial_id_);
        visible = v;
        return wait_stable_and_execute_read(visible);
      } else {
        // visible version not found in global array
        visible = val->master_;
        return execute_read(visible);
      }

    } else if (val->has_dirty_region()) {
      auto [is_found, core, tx] = val->row_region_->identify_visible_version(
          core_, get_tx_serial(serial_id_));

      if (is_found) {
        assert((core * 64 + tx) < serial_id_);
        visible = val->row_region_->arrays_[core]->get(tx);
        return wait_stable_and_execute_read(visible);
      } else {
        // visible version not found in per core version array
        visible = val->master_;
        return execute_read(visible);
      }
    }

    // initialized by major gc and no write occur in the epoch
    visible = val->master_;
    return execute_read(visible);
  }

  Rec *write(TableID table_id, Version *pending) {
    return upsert(table_id, pending);
  }

  Rec *upsert(TableID table_id, Version *pending) {
    const Schema &sch = Schema::get_schema();
    size_t record_size = sch.get_record_size(table_id);

    Rec *rec = reinterpret_cast<Rec *>(operator new(record_size));
    __atomic_store_n(&pending->rec, rec, __ATOMIC_SEQ_CST);  // write
    __atomic_store_n(&pending->status, Version::VersionStatus::STABLE,
                     __ATOMIC_SEQ_CST);

    return rec;
  }

  uint64_t core_;
  uint64_t serial_id_;  // 0 - 4096
  uint64_t epoch_ = 0;

 private:
  WriteSet<Key> ws;  // write set
  std::set<TableID> tables;

  RowRegion *spare_region_ = nullptr;

  RowRegionController &rrc_;

  Stat &stat_;

  MajorGC &major_gc_;

  void do_append_pending_version(Value *val, Version *&pending) {
    assert(!pending);

    epoch_guard(val);

    assert(val->epoch_ == epoch_);
    /*
    val->epoch_ == epoch_
    */

    // the val is may be uncontented
    if (val->try_lock()) {
      // Successfully got the try lock

      // the val will or may be contented if the row_region_ is already
      // exist.
      RowRegion *cur_region =
          __atomic_load_n(&val->row_region_, __ATOMIC_SEQ_CST);
      if (may_be_contented(cur_region)) {
        pending = append_to_contented_row(val, cur_region);
      } else {
        pending = append_to_unconted_row(val);
      }

      val->unlock();
      return;
    }

    // couldn't acquire the lock: the val is getting crowded.
    RowRegion *region = wait_region_installation(val);
    assert(region);
    pending = append_to_contented_row(val, region);
    return;
  }

  RowRegion *wait_region_installation(Value *val) {
    RowRegion *region;
    uint64_t start = rdtscp();
    while (!(region = __atomic_load_n(&val->row_region_, __ATOMIC_SEQ_CST))) {
      if (val->try_lock()) {
        stat_.add(Stat::MeasureType::WaitInInitialization, rdtscp() - start);
        // region_を設置する権限を得る。他のスレッドは、region_が設置されるまで待機
        region = __atomic_load_n(&val->row_region_, __ATOMIC_SEQ_CST);
        if (!region) {
          assert(!__atomic_load_n(&val->row_region_, __ATOMIC_SEQ_CST));
          region = rrc_.fetch_new_region();
          move_global_array_to_row_region(val->global_array_, region);
          __atomic_store_n(
              &val->row_region_, region,
              __ATOMIC_SEQ_CST);  // これした時点でunlockする前に、他のスレッドは、regionに触る可能性がある
        }  // else: other thread already install region
        val->unlock();
        return region;
      }
    }
    stat_.add(Stat::MeasureType::WaitInInitialization, rdtscp() - start);
    return region;  // other thread already install region
  }

  uint64_t get_core_serial(uint64_t serial_id) { return serial_id / 64; }
  uint64_t get_tx_serial(uint64_t serial_id) { return serial_id % 64; }
  std::pair<uint64_t, uint64_t> decompose_id_serial(uint64_t serial_id) {
    return {get_core_serial(serial_id), get_tx_serial(serial_id)};
  }

  void move_global_array_to_row_region(GlobalVersionArray &g_array,
                                       RowRegion *region) {
    for (auto [serial_id, version] : g_array.ids_slots_) {
      auto [core, tx] = decompose_id_serial(serial_id);
      region->append(core, version, tx, stat_);
    }
    g_array.ids_slots_.clear();
  }

  void epoch_guard(Value *val) {
    uint64_t start = rdtscp();
    while (__atomic_load_n(&val->epoch_, __ATOMIC_SEQ_CST) < epoch_) {
      if (val->try_lock()) {
        stat_.add(Stat::MeasureType::WaitInInitialization, rdtscp() - start);
        if (__atomic_load_n(&val->epoch_, __ATOMIC_SEQ_CST) < epoch_) {
          // the first transaction in the current epoch to arrive on
          // the val
          val->initialize_the_row(epoch_, stat_);
        }
        val->unlock();
        return;
      }
    };
    stat_.add(Stat::MeasureType::WaitInInitialization, rdtscp() - start);
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
    }
    stat_.add(Stat::MeasureType::WaitInExecution, rdtscp() - start);
    return execute_read(visible);
  }

  // lock should be acuired before this function is called
  Version *append_to_unconted_row(Value *val) {
    Version *new_version = create_pending_version(val);

    // Append pending version to global version chain
    val->global_array_.append(new_version, serial_id_);
    assert(val->global_array_.is_exist(serial_id_));

    return new_version;
  }

  bool may_be_contented(RowRegion *row_region) { return row_region != nullptr; }

  Version *append_to_contented_row(Value *val, RowRegion *region) {
    // 1. Create pending version
    Version *pending = create_pending_version(val);

    // 2. Append to per-core version array and update transaction bitmap
    region->append(core_, pending, get_tx_serial(serial_id_), stat_);

    return pending;
  }

  Version *create_pending_version([[maybe_unused]] Value *val) {
    // Version *version = reinterpret_cast<Version *>(
    //     MemoryAllocator::aligned_allocate(sizeof(Version)));
    Version *version = new Version;
    version->rec = nullptr;  // TODO
    version->deleted = false;
    version->status = Version::VersionStatus::PENDING;

    stat_.increment(Stat::MeasureType::Create);

    // major_gc_.collect(epoch_, val);

    return version;
  }
};