#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "protocols/common/schema.hpp"

using Rec = void;

template <typename Key>
class WriteSet {
 public:
  std::vector<Key>& get_table(TableID table_id) { return ws[table_id]; }

 private:
  std::unordered_map<TableID, std::vector<Key>> ws;
};