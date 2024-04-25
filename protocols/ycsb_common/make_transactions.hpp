#pragma once

#include <vector>

#include "benchmarks/ycsb/include/config.hpp"

template <typename OperationSet>
void make_transactions(std::vector<OperationSet>& txs) {
  const Config& c = get_config();

  for (uint64_t tx_id = 0; tx_id < txs.size(); tx_id++) {
    OperationSet& tx = txs[tx_id];

    for (uint64_t j = 0; j < c.get_reps_per_txn(); j++) {
      int operationType = urand_int(1, 100);
      int key = zipf_int(c.get_contention(), c.get_num_records());

      if (operationType <= c.get_read_propotion()) {
        tx.rw_set_.emplace_back(Operation::Ope::Read, key);
      } else {
        tx.rw_set_.emplace_back(Operation::Ope::Update, key);
        tx.w_set_.emplace_back(Operation::Ope::Update, key);
      }
    }
  }
}