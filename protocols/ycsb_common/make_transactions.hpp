#pragma once

#include <vector>

template <typename OperationSet>
void make_transactions(std::vector<OperationSet> &txs) {
    for (uint64_t tx_id = 0; tx_id < txs.size(); tx_id++) {
        txs[tx_id].make_operations();
    }
}