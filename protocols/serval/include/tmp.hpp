void append_pending_version_old(TableID table_id, Key key) {
  Index& idx = Index::get_index();

  tables.insert(table_id);
  auto& rw_table = rws.get_table(table_id);
  auto rw_iter = rw_table.find(key);

  Version* pending_version = nullptr;

  // Case of not read and written
  if (rw_iter == rw_table.end()) {
    Value* val;
    typename Index::Result res =
        idx.find(table_id, key, val);  // find corresponding index in
    masstree

        if (res == Index::Result::NOT_FOUND) {
      throw std::runtime_error(
          "masstree NOT_FOUND");  // TODO: この場合、どうするかを考える
      return;
    }

    // Got value from masstree
    RowRegion* cur_region =
        __atomic_load_n(&val->row_region_, __ATOMIC_SEQ_CST);

    if (may_be_uncontented(cur_region)) {
      if (val->try_lock()) {
        // Successfully got the try lock (00)
        pending_version = append_to_unconted_row(val);
      } else {
        // this row is contented
        RowRegion* new_region =
            spare_region_ ? spare_region_ : rrc_.fetch_new_region();

        if (try_install_new_region(val, cur_region, new_region)) {
          pending_version = append_to_contented_row(val, new_region);
        } else {
          // other thread already install new region
          RowRegion* other_region =
              __atomic_load_n(&val->row_region_, __ATOMIC_SEQ_CST);
          spare_region_ = new_region;
          pending_version = append_to_contented_row(val, other_region);
        }
      }
    } else {
      // region is not nullptr -> Per-core version array exist (_1)
      pending_version = append_to_contented_row(val, cur_region);
    }

    assert(pending_version);

    // Place it into readwriteset
    auto new_iter = rw_table.emplace_hint(
        rw_iter, std::piecewise_construct, std::forward_as_tuple(key),
        std::forward_as_tuple(pending_version->rec, nullptr,
                              ReadWriteType::UPDATE, false, val,
                              pending_version));  // TODO: チェックする
    // Place it in writeset
    auto& w_table = ws.get_table(table_id);
    w_table.emplace_back(key, new_iter);
  }

  // TODO: Case of found in read and written set
}

void read([[maybe_unused]] TableID table_id, [[maybe_unused]] Key key) {
  /*

   Index& idx = Index::get_index();

   Value* val;
   typename Index::Result res =
       idx.find(table_id, key, val);  // find corresponding index in
       masstree

   if (res == Index::Result::NOT_FOUND) {
     throw std::runtime_error(
         "masstree NOT_FOUND");  // TODO: この場合、どうするかを考える
     return nullptr;
   }

   // Use val to find visible version

   // ====== 1. load core bitmap and identify the core which create the
   visible
   // version ======

   // load current core's core bitmap
   uint64_t current_core_bitmap = val->core_bitmap_;

   // my_core_pos -> spot the current core's digit in core bitmap
   (range: 0-63) int my_core_pos = core_ % 64;

   // latest_core_pos -> spot the core that occurs the latest update
   int latest_core_pos =
       mask_and_find_latest_pos(current_core_bitmap, my_core_pos);

   // return nullptr if no visible version available
   if (latest_core_pos == 0) {
     return nullptr;
   }
   //
   　===========================================================================================

   // ====== 2. load the transaction bitmap from the identified core
   ======

   // load current per-core version array which potentially has visible
   Version PerCoreVersionArray* current_version_array =
       val->row_region_->arrays_[latest_core_pos];
   //
   　===========================================================================================

   // ====== 3. identify visible version by calculating
   transaction_bitmap
   // ======

   // load curent transaction bitmap from current version array
   uint64_t current_transaction_bitmap =
       current_version_array->transaction_bitmap_;

   // my_tx_pos -> spot the current tx's digit in tx bitmap
   int my_tx_pos = static_cast<int>(txid_.thread_id) % 64;

   // calculate latest_tx_num
   int latest_tx_pos =
       mask_and_find_latest_pos(current_transaction_bitmap, my_tx_pos);

   // the current per-core version array does not have the visible
   version,
   // needs to find visible version from the previous core
   if (latest_tx_pos == 0) {
   AGAIN:
     // load previous core's version array and transaction bitmap
     PerCoreVersionArray* previous_version_array =
         val->row_region_->arrays_[latest_core_pos - 1];
     uint64_t previous_transaction_bitmap =
         previous_version_array->transaction_bitmap_;

     // find the visible version from previous core (no mask needed)
     int previous_latest_tx_pos =
   find_latest_pos(previous_transaction_bitmap); if
   (previous_latest_tx_pos
   == 0) { goto AGAIN;
     }
     Version* previous_latest_version =
         previous_version_array->slots_[previous_latest_tx_pos];
     return previous_latest_version->rec;
   }

   Version* latest_version =
   current_version_array->slots_[latest_tx_pos];
   //
   ===========================================================================================

   return latest_version->rec;
    */
}

// returns the number of leading "0" from left
template <typename T>
int num_leading_zeros(T x) {
  static_assert(std::is_same_v<T, uint64_t>, "T must be uint64_t");
  return __builtin_clzll(x);
}

// returns the number of trailing "0" from left
template <typename T>
int num_trailing_zeros(T x) {
  static_assert(std::is_same_v<T, uint64_t>, "T must be uint64_t");
  return __builtin_ctzll(x);
}

// find the most-left "1" after clearing all bits from pos with mask
int mask_and_find_latest_pos(uint64_t bits, int pos) {
  uint64_t masked_bits = clear_left(bits, pos);
  return (63 - num_leading_zeros(masked_bits));
}

// find the most-left "1" after clearing all bitswithout masking
int find_latest_pos(uint64_t bits) { return (63 - num_leading_zeros(bits)); }

uint64_t clear_left(uint64_t bits, int pos) {
  uint64_t mask = (1ULL << (pos + 1)) - 1;
  return bits & mask;
}

uint64_t clear_right(uint64_t bits, int pos) {
  uint64_t mask = ~((1ULL << pos) - 1);
  return bits & mask;
}