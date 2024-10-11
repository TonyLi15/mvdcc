#pragma once

#include <cassert>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "protocols/caracal/include/row_buffer.hpp"
#include "protocols/caracal/include/version.hpp"
#include "protocols/ycsb_common/definitions.hpp"

#include "utils/tsc.hpp"

class MajorGC {
 public:
  void collect(uint64_t cur_epoch, GlobalVersionArray *array) {
    arrays_[cur_epoch].insert(array);
  }

  void major_gc(uint64_t new_epoch, Stat &stat) {
    if (arrays_.empty()) return;

    auto itr = arrays_.begin();
    while (itr != arrays_.end()) {
      auto [epoch, g_arrays] = *itr;
      if ((new_epoch - k_) <= epoch) break;

      // (new_epoch - k_) > epoch
      // であるようなg_arrayにアクセスしてGCを行う

      for (auto &g_array : g_arrays) {
        uint64_t start = rdtscp();
        g_array->lock();
        stat.add(Stat::MeasureType::WaitInGC, rdtscp() - start);
        g_array->minor_gc(new_epoch, stat);
        g_array->unlock();
      }

      itr = arrays_.erase(itr);  // TODO: 再考
    }
  }

 private:
  static const int k_ = 4;

  // <epoch, ptr of array_list>
  std::map<uint64_t, std::unordered_set<GlobalVersionArray *>> arrays_;
};