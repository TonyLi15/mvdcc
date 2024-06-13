#pragma once

#include <vector>

#include "benchmarks/ycsb/include/config.hpp"
#include "value.hpp"

class Operation {
  public:
    enum Ope { Read, Update };
    Ope ope_;
    uint64_t index_;
    uint64_t value_ = 0;

    Version *pending_ = nullptr;
    // pending version is installed in initialization phase
    // used in execution phase for write

    Operation(Ope operation, uint64_t idx) : ope_(operation), index_(idx) {}
};

/*
 3 of the rows are chosen from the entire database and the remaining 7 rows are
 chosen from a small set of 77 rows that are spaced 217 apart in the 10M key
 space. The 3 keys and the 7 keys are chosen using either a uniform or a skewed
 distribution from their respective set. These workloads trigger Caracal’s
 contention optimizations.
 */
class OperationSet {
  public:
    std::vector<Operation *> rw_set_;
    std::vector<Operation *> w_set_;

    OperationSet() {
        std::vector<int> contented_keys;
        for (int i = 0; i < 77; i++) {
            contented_keys.emplace_back(131072 * i);
        }

        const Config &c = get_config();

        for (uint64_t j = 0; j < 3; j++) {
            int operationType = urand_int(1, 100);
            int three_of_ten_key =
                zipf_int(c.get_contention(), c.get_num_records());
            Operation *ope;
            if (operationType <= c.get_read_propotion()) {
                // ope = new (MemoryAllocator::allocate(sizeof(Operation)))
                //     Operation(Operation::Ope::Read, key);
                ope = new Operation(Operation::Ope::Read, three_of_ten_key);
            } else {
                // ope = new (MemoryAllocator::allocate(sizeof(Operation)))
                //     Operation(Operation::Ope::Update, key);
                ope = new Operation(Operation::Ope::Update, three_of_ten_key);
                w_set_.emplace_back(ope);
            }
            rw_set_.emplace_back(ope);
        }

        for (uint64_t j = 0; j < 7; j++) {
            int operationType = urand_int(1, 100);
            int seven_of_ten_key = contented_keys
                [zipf_int(c.get_contention(), c.get_num_records()) % 77];
            // contented_keys[urand_int(0, 76)]; // TODO: change
            Operation *ope;
            if (operationType <= c.get_read_propotion()) {
                // ope = new (MemoryAllocator::allocate(sizeof(Operation)))
                //     Operation(Operation::Ope::Read, key);
                ope = new Operation(Operation::Ope::Read, seven_of_ten_key);
            } else {
                // ope = new (MemoryAllocator::allocate(sizeof(Operation)))
                //     Operation(Operation::Ope::Update, key);
                ope = new Operation(Operation::Ope::Update, seven_of_ten_key);
                w_set_.emplace_back(ope);
            }
            rw_set_.emplace_back(ope);
        }
    }

    ~OperationSet() {
        for (uint64_t i = 0; i < rw_set_.size(); i++) {
            // MemoryAllocator::deallocate(rw_set_[i]);
            delete rw_set_[i];
        }
    }
};