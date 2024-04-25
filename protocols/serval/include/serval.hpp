#pragma once

#include <cassert>
#include <cstring>
#include <set>
#include <stdexcept>

#include "indexes/masstree.hpp"
#include "protocols/common/timestamp_manager.hpp"
#include "protocols/common/transaction_id.hpp"
#include "protocols/serval/include/readwriteset.hpp"
#include "protocols/serval/include/region.hpp"
#include "protocols/serval/include/value.hpp"
#include "utils/bitmap.hpp"
#include "utils/logger.hpp"
#include "utils/utils.hpp"

template <typename Index>
class Serval {
 public:
  using Key = typename Index::Key;
  using Value = typename Index::Value;
  using Version = typename Value::Version;
  using LeafNode = typename Index::LeafNode;
  using NodeInfo = typename Index::NodeInfo;

  Serval(uint64_t core_id, uint64_t txid, RowRegionController& rrc)
      : core_(core_id), txid_(txid), rrc_(rrc) {}

  ~Serval() {}

  void terminate_transaction() {
    for (TableID table_id : tables) {
      auto& w_table = ws.get_table(table_id);
      w_table.clear();
    }
    tables.clear();
  }

  void append_pending_version(TableID table_id, Key key, Version*& pending) {
    Index& idx = Index::get_index();

    tables.insert(table_id);
    std::vector<Key>& w_table = ws.get_table(table_id);
    typename std::vector<Key>::iterator w_iter =
        std::find(w_table.begin(), w_table.end(), key);

    // Case of not append occur
    if (w_iter == w_table.end()) {
      Value* val;
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
      auto& w_table = ws.get_table(table_id);
      w_table.emplace_back(key);
    }

    // TODO: Case of found in read or written set
  }

  const Rec* read(TableID table_id, Key key) {
    Index& idx = Index::get_index();

    Value* val;
    typename Index::Result res =
        idx.find(table_id, key, val);  // find corresponding index in masstree

    if (res == Index::Result::NOT_FOUND) {
      assert(false);
      throw std::runtime_error(
          "masstree NOT_FOUND");  // TODO: この場合、どうするかを考える
      return nullptr;
    }

    Version* visible = nullptr;

    // any append is not executed in the row in the current epoch
    // and read the final state in one previous epoch
    uint64_t epoch = val->epoch_;
    if (epoch != epoch_) {
      visible = val->master_;
      return execute_read(visible);
    }

    assert(epoch == epoch_);

    uint64_t core_bitmap = val->core_bitmap_;

    auto [txid_in_global_array, visible_in_global_array] =
        val->global_array_.search_visible_version(txid_);

    // any append did not occur in the epoch
    if (core_bitmap == 0 && !visible_in_global_array) {
      visible = val->master_;
      return execute_read(visible);
    }

    // append occur only in global array
    if (core_bitmap == 0 && visible_in_global_array) {
      assert(visible_in_global_array);
      assert(txid_in_global_array < txid_);
      visible = visible_in_global_array;
      assert(visible);
      return wait_stable_and_execute_read(visible);
    }

    assert(val->row_region_);  // 以下は core_bitmap != 0 のとき

    auto [core, tx] = identify_visible_version_in_per_core_version_array(val);

    // append occur only in per core version array
    if (core_bitmap != 0 && !visible_in_global_array) {
      if (core == -1) {
        // visible version not found in per core version array
        visible = val->master_;
        return execute_read(visible);
      } else {
        visible = val->row_region_->arrays_[core]->get(tx);
        return wait_stable_and_execute_read(visible);
      }
    };

    // append occur in per core version array and in global array
    if (core_bitmap != 0 && visible_in_global_array) {
      if (core == -1) {
        // visible version not found in per core version array
        visible = visible_in_global_array;
        return wait_stable_and_execute_read(visible);
      } else {
        uint64_t tx_id = (core * 64) + tx;
        visible = (tx_id < txid_in_global_array)
                      ? visible_in_global_array
                      : val->row_region_->arrays_[core]->get(tx);
        return wait_stable_and_execute_read(visible);
      }
    }

    assert(false);
    return nullptr;
  }

  std::tuple<int, int> identify_visible_version_in_per_core_version_array(
      Value* val) {
    int core = find_the_largest_among_smallers(val->core_bitmap_, core_);
    if (core == -1) return {-1, -1};  // not found in the per core version array

    int tx = find_the_largest_among_smallers(
        val->row_region_->arrays_[core]->transaction_bitmap_, txid_ % 64);
    if (tx != -1) return {core, tx};  // found in the per core version array

    if (core == 0) return {-1, -1};  // not found in the per core version array
    int next_core =
        find_the_largest_among_smallers(val->core_bitmap_, core - 1);
    if (next_core == -1)
      return {-1, -1};  // not found in the per core version array

    int next_tx = find_the_largest(
        val->row_region_->arrays_[next_core]->transaction_bitmap_);  //
    assert(0 <= next_tx);
    return {next_core, next_tx};  // found in the per core version array
  }

  Rec* write(TableID table_id, Version* pending) {
    return upsert(table_id, pending);
  }

  Rec* upsert(TableID table_id, Version* pending) {
    const Schema& sch = Schema::get_schema();
    size_t record_size = sch.get_record_size(table_id);

    Rec* rec = MemoryAllocator::aligned_allocate(record_size);

    __atomic_store_n(&pending->rec, rec, __ATOMIC_SEQ_CST);  // write
    __atomic_store_n(&pending->status, Version::VersionStatus::STABLE,
                     __ATOMIC_SEQ_CST);
    return rec;
  }

  uint64_t core_;
  // TxID txid_;
  uint64_t txid_;
  uint64_t epoch_ = 0;

 private:
  WriteSet<Key> ws;  // write set
  std::set<TableID> tables;

  RowRegion* spare_region_ = nullptr;

  RowRegionController& rrc_;

  void do_append_pending_version(Value* val, Version*& pending) {
    assert(!pending);

    epoch_guard(val, pending);
    if (pending) return;

    /*
    val->epoch_ == epoch_
    */
    RowRegion* cur_region =
        __atomic_load_n(&val->row_region_, __ATOMIC_SEQ_CST);

    // the val will or may be contented if the row_region_ is already exist.
    if (may_be_contented(cur_region)) {
      pending = append_to_contented_row(val, cur_region);
      return;
    }

    // the val is may be uncontented
    if (val->try_lock()) {
      // Successfully got the try lock (00)
      pending = append_to_unconted_row(val);
      return;
    }

    // couldn't acquire the lock: the val is getting crowded.
    RowRegion* new_region;

    if (spare_region_) {
      new_region = spare_region_;
      spare_region_ = nullptr;
    } else {
      new_region = rrc_.fetch_new_region();
    }

    assert(cur_region == nullptr);
    if (try_install_new_region(val, cur_region, new_region)) {
      pending = append_to_contented_row(val, new_region);
      return;
    }

    // other thread already install new region
    RowRegion* other_region =
        __atomic_load_n(&val->row_region_, __ATOMIC_SEQ_CST);
    spare_region_ = new_region;
    pending = append_to_contented_row(val, other_region);
    return;
  }

  void epoch_guard(Value* val, Version*& pending) {
    uint64_t epoch;

    while ((epoch = __atomic_load_n(&val->epoch_, __ATOMIC_SEQ_CST)) < epoch_) {
      /*
       case1: val->epoch_ < epoch_
       */
      assert(epoch < epoch_);

      if (!val->try_lock()) continue;

      assert(val->epoch_ <= epoch_);
      if (val->epoch_ < epoch_) {
        // the first transaction in the current epoch to arrive on the val
        pending = initialize_the_row_and_append(val);  // unlock
      } else {
        pending = append_to_unconted_row(val);  // unlock
      }

      return;
    };
  }

  Version* initialize_the_row_and_append(Value* val) {
    // 1. store the final state of one previous epoch to val->master_
    val->master_ = find_final_state_in_one_previous_epoch(val);
    assert(val->master_);

    // 2. initialize the global_array_ and core_bitmap_
    val->core_bitmap_ = 0;
    val->global_array_.initialize();

    // 3. append to the global_array_
    Version* pending = create_pending_version();
    val->global_array_.append(pending, txid_, core_);

    // 4. update val->epoch_
    val->epoch_ = epoch_;
    assert(val->global_array_.is_exist(txid_));

    val->unlock();

    return pending;
  }

  // call with lock
  Version* find_final_state_in_one_previous_epoch(Value* val) {
    auto [id_global_array, latest_global_array] = val->global_array_.latest();

    assert(latest_global_array);

    // any append didn't occurred in one previous epoch
    assert(!(latest_global_array == nullptr && val->core_bitmap_ == 0));

    // only appended in global array
    if (latest_global_array != nullptr && val->core_bitmap_ == 0) {
      assert(latest_global_array);
      return latest_global_array;
    }

    assert(val->row_region_);  // 以下は、val->core_bitmap_ != 0 のとき

    // core_bitmap_ is true
    int core = find_the_largest(val->core_bitmap_);
    PerCoreVersionArray* version_array = val->row_region_->arrays_[core];

    // only appended in region
    if (latest_global_array == nullptr && val->core_bitmap_ != 0) {
      assert(0 < version_array->length());
      assert(version_array->latest());
      return version_array->latest();
    }

    // appended in core_bitmap_ and global array
    if (latest_global_array != nullptr && val->core_bitmap_ != 0) {
      int tx = find_the_largest(version_array->transaction_bitmap_);
      uint64_t tx_id = (core * 64) + tx;
      assert(tx_id != id_global_array);

      if (tx_id < id_global_array) {
        assert(latest_global_array);
        return latest_global_array;
      } else {
        assert(0 < version_array->length());
        assert(version_array->latest());  // ERROR
        return version_array->latest();
      }
    }

    assert(false);
    return nullptr;
  }

  Rec* execute_read(Version* visible) {
    assert(visible);
    return visible->rec;
  }

  Rec* wait_stable_and_execute_read(Version* visible) {
    assert(visible);
    // while (__atomic_load_n(&visible->status, __ATOMIC_SEQ_CST) ==
    //        Version::VersionStatus::PENDING) {
    //   // spin
    // }
    // TODO: やばい
    return execute_read(visible);
  }

  bool try_install_new_region(Value* val, RowRegion* cur_region,
                              RowRegion* new_region) {
    assert(cur_region == nullptr);
    return __atomic_compare_exchange_n(&val->row_region_, &cur_region,
                                       new_region, false, __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST);
  }

  // lock should be acuired before this function is called
  Version* append_to_unconted_row(Value* val) {
    Version* new_version = create_pending_version();

    // Append pending version to global version chain
    val->global_array_.append(new_version, txid_, core_);
    assert(val->global_array_.is_exist(txid_));

    val->unlock();

    return new_version;
  }

  bool may_be_contented(RowRegion* row_region) { return row_region != nullptr; }

  Version* append_to_contented_row(Value* val, RowRegion* region) {
    // 1. Create pending version
    Version* pending = create_pending_version();

    // 2. Update core bitmap if this is the first append in the current epoch
    // Otherwise, core_bitmap_ is already updated
    uint64_t core_bitmap =
        __atomic_load_n(&val->core_bitmap_, __ATOMIC_SEQ_CST);
    if (!is_bit_set_at_the_position(core_bitmap, core_)) {
      region->initialize(core_);  // clear transaction bitmap
      __atomic_or_fetch(&val->core_bitmap_,
                        set_bit_at_the_given_location(core_),
                        __ATOMIC_SEQ_CST);  // core 2: 0010 0000 ... 0000
      assert(is_bit_set_at_the_position(
          __atomic_load_n(&val->core_bitmap_, __ATOMIC_SEQ_CST), core_));
    }

    // 3. Append to per-core version array and update transaction bitmap
    region->append(core_, pending, txid_ % 64);

    return pending;
  }

  Version* create_pending_version() {
    Version* version = reinterpret_cast<Version*>(
        MemoryAllocator::aligned_allocate(sizeof(Version)));
    version->rec = nullptr;  // TODO
    version->deleted = false;
    version->status = Version::VersionStatus::PENDING;
    return version;
  }
};