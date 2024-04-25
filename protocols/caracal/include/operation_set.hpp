#pragma once

#include <vector>

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

class OperationSet {
 public:
  std::vector<Operation> rw_set_;
  std::vector<Operation> w_set_;
};