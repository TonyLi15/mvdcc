#pragma once

#include <cassert>
#include <cstdint>
#include <unordered_set>

#include "protocols/serval/include/value.hpp"
#include "protocols/ycsb_common/definitions.hpp"
#include "utils/tsc.hpp"

class MajorGC {
 public:
  void collect(uint64_t cur_epoch, Value *value) {
    values_[cur_epoch].insert(value);
  }

  void major_gc(uint64_t core, uint64_t new_epoch, Stat &stat) {
    if (values_.empty()) return;

    auto itr = values_.begin();
    while (itr != values_.end()) {
      auto [epoch, vals] = *itr;
      if ((new_epoch - k_) <= epoch) break;

      // (new_epoch - k_) > epoch
      // であるようなg_arrayにアクセスしてGCを行う
      for (auto &val : vals) {
        uint64_t val_epoch = __atomic_load_n(&val->epoch_, __ATOMIC_SEQ_CST);

        /*
        [val_epoch == new_epoch]
        global arrayは完全にGC済み
        => per core version　arrayには誰も来ない
        自分のper core version　arrayのみをGC (no latch)
        * identify masterする必要はない

        [val_epoch < new_epoch]
        global arrayをGC (latch)
        自分のper core version　arrayのみをGC (latch)
        * identify masterする必要がある

        [val_epoch > new_epoch]
        assert false
        */
        assert(val_epoch <= new_epoch);

        if (val_epoch == new_epoch) {
          // 自分のper core version　arrayのみをGC
          if (val->row_region_) {
            val->row_region_->gc_and_initialize_tx_bitmap(core, stat);
          }
        } else {
          uint64_t start = rdtscp();
          val->lock();
          stat.add(Stat::MeasureType::WaitInGC, rdtscp() - start);
          // 再度チェック
          if (__atomic_load_n(&val->epoch_, __ATOMIC_SEQ_CST) == new_epoch) {
            val->unlock();
            // 自分のper core version　arrayのみをGC
            if (val->row_region_) {
              val->row_region_->gc_and_initialize_tx_bitmap(core, stat);
            }
          } else {
            // global arrayをGC
            // 自分のper core version　arrayのみをGC
            val->initialize_the_row(new_epoch, stat);
            if (val->row_region_) {
              val->row_region_->gc_and_initialize_tx_bitmap(core, stat);
            }
            val->unlock();
          }
        }
      }

      itr = values_.erase(itr);  // TODO: 再考
    }
  }

 private:
  static const int k_ = 4;

  // <epoch, ptr of value_list>
  std::map<uint64_t, std::unordered_set<Value *>> values_;
};