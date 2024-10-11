#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "benchmarks/ycsb/include/config.hpp"
#include "protocols/cheetah/include/version.hpp"
#include "protocols/common/readwritelock.hpp"
#include "utils/bitmap.hpp"

class WriteBitmap {
 public:
  // incremented in read phase, decremented in write phase
  alignas(64) uint64_t ref_cnt_ = 0;  // #readers + 1

  alignas(64) RWLock lock_;

  /*
   written in write phase by writers.
   read in read phase by readers.
   cleared in execution phase by final writer
  */
  uint64_t core_bitmap_ = 0;
  std::vector<uint64_t> tx_bitmaps_;

  /*
  version append in read phase by readers.
  gc in exec phase by final reader.
  */
  std::unordered_map<uint64_t, Version *> placeholders_;

  /*
  update in execution phase by final writer.
  */
  Version *master_ = nullptr;  // final state in one previous epoch
  Version *previous_master_ = nullptr;

  // write phase: should be called with lock
  void update_bitmap(uint64_t core, uint64_t tx_bitmap) {
    int insert_pos = count_prefix_sum(core);
    assert(!is_bit_set_at_the_position(core_bitmap_, core));
    insert_tx_bitmap(insert_pos, tx_bitmap);
    update_core_bitmap(core);
    assert(is_bit_set_at_the_position(core_bitmap_, core));
  }

  /*
  read phase
  */
  Version *append_pending_version(uint64_t core, uint64_t tx, Stat &stat) {
    uint64_t ref_cnt = __atomic_fetch_add(&ref_cnt_, 1, __ATOMIC_SEQ_CST);
    if (ref_cnt == 0) {
      __atomic_fetch_add(&ref_cnt_, 1, __ATOMIC_SEQ_CST);  // for #readers + 1
    }

    if (core_bitmap_) {  // write occur in this epoch
      auto [is_found, visible_core, visible_tx] =
          identify_visible_version_in_bitmaps(core, tx);

      if (is_found) {  // read the version created in this epoch
        uint64_t visible_id = get_serial_id(visible_core, visible_tx);

        // ***************** should be atomic *****************
        lock_.lock();
        Version *v = placeholders_[visible_id];
        if (!v) {  // visible_id is not exsit in placeholders_
          v = create_pending_version(stat);  // placeholderを新規作成
          placeholders_[visible_id] = v;
        }
        lock_.unlock();
        // ***************** should be atomic *****************

        assert(v);
        return v;
      }
      assert(master_->status == Version::VersionStatus::STABLE);
      assert(master_);
      return master_;  // read the master version
    } else {
      // for debug
      assert(placeholders_.empty());
    }
    assert(master_);
    assert(master_->status == Version::VersionStatus::STABLE);
    return master_;  // this is read only record
  }

  // execute read
  void decrement_ref_cnt([[maybe_unused]] Stat &stat) {
    [[maybe_unused]] uint64_t ref_cnt =
        __atomic_sub_fetch(&ref_cnt_, 1, __ATOMIC_SEQ_CST);

    if (ref_cnt == 1) {
      // ***************** should be atomic *****************
      lock_.lock();
      // 1. gc previous master
      if (previous_master_) {  // final writer lock < final reader lock
        gc(previous_master_, stat);
        assert(master_);
      }

      // if any append in placeholders occur...
      // 2. gc placeholders
      if (!placeholders_.empty()) {
        // last writerのversionは絶対に含まれていないことは保証されている
        for (auto &[id, version] : placeholders_) {
          assert(version->status == Version::VersionStatus::STABLE);
          gc(version, stat);
        }
        placeholders_.clear();
      }

      assert(previous_master_ == nullptr);
      assert(placeholders_.empty());
      __atomic_sub_fetch(&ref_cnt_, 1, __ATOMIC_SEQ_CST);
      // once gc by final reader finish, ref_cnt_ become 0.
      lock_.unlock();
      // ***************** should be atomic *****************
    }
  }

  // execute write
  Version *identify_write_version(uint64_t core, uint64_t tx,
                                  [[maybe_unused]] Stat &stat) {
    Version *version = nullptr;
    uint64_t serial_id = get_serial_id(core, tx);

    lock_.lock();
    auto itr = placeholders_.find(serial_id);

    if (is_final_state(core, tx)) {
      // ***************** should be atomic *****************

      // 1. final writer should create final version
      if (itr == placeholders_.end()) {
        version = create_pending_version(stat);
      } else {  // final version is already created in the placeholders_
        version = itr->second;
        placeholders_.erase(itr);
        // 今後のreaderが間違ってfinal
        // versionをGCしないようにplaceholders_から削除しておく
      }

      /*
       2. final writer is responsible for...
       - A. when final writer comes after final reader
       - or in the write only workload, (i.e. ref_cnt == 0)
       - final writer should gc master_ and assign final state to master_.
       - B. if any reader will come after the final writer, (i.e. 0 < ref_cnt)
       - final writer should stash current master_ to previous_master_.
       - (because some reader is using current master_)
       - then, assign final state to master_.
      */
      if (__atomic_load_n(&ref_cnt_, __ATOMIC_SEQ_CST) ==
          0) {  // final reader lock < final writer lock or write only
        // gc master
        gc(master_, stat);  // もう誰からも読まれない
        assert(placeholders_.empty());
      } else {
        previous_master_ = master_;  // 今後のreaderのために残しておく
      }
      master_ = version;  // assign final state to master_

      // 3. final writer is responsible for clear bitmaps.
      if (core_bitmap_) {
        clear_bitmaps();
      }

      assert(core_bitmap_ == 0);
      assert(tx_bitmaps_.empty());

      // ***************** should be atomic *****************

    } else {
      if (itr != placeholders_.end()) {
        version = itr->second;
      }
    }
    lock_.unlock();

    return version;
  }

  int count_prefix_sum(uint64_t core) {
    /*
    core: 4
    0010 [1]000 ...: prefix_sum　is 1

    core: 4
    0011 [1]000 ...: prefix_sum　is is 2
    */
    return count_bits(core_bitmap_ &
                      fill_the_left_side_until_before_the_given_position(core));
  }

 private:
  void gc(Version *&version, Stat &stat) {
    assert(version);
    assert(version->status == Version::VersionStatus::STABLE);
    delete reinterpret_cast<Record *>(version->rec);
    delete version;
    stat.increment(Stat::MeasureType::Delete);
    version = nullptr;
  }

  uint64_t get_serial_id(uint64_t core, uint64_t tx) { return core * 64 + tx; }

  void clear_bitmaps() {
    core_bitmap_ = 0;
    tx_bitmaps_.clear();
  }
  // execute write
  bool is_final_state(uint64_t core, uint64_t tx) {
    bool is_final = false;
    if (core_bitmap_) {
      uint64_t last_core = find_the_largest(core_bitmap_);
      if (last_core == core) {
        assert(tx_bitmaps_.back());
        uint64_t last_tx = find_the_largest(tx_bitmaps_.back());
        if (last_tx == tx) is_final = true;
      }
    }
    return is_final;
  }

  void update_tx_bitmap(int pos, uint64_t tx) {
    assert(pos < (int)tx_bitmaps_.size());
    tx_bitmaps_[pos] = set_bit_at_the_given_location(tx_bitmaps_[pos], tx);
  }
  void insert_tx_bitmap(int insert_pos, uint64_t tx_bitmap) {
    tx_bitmaps_.insert(tx_bitmaps_.begin() + insert_pos, tx_bitmap);
  }
  void update_core_bitmap(uint64_t core) {
    core_bitmap_ = set_bit_at_the_given_location(core_bitmap_, core);
  }

  // read phase
  std::tuple<bool, uint64_t, uint64_t> identify_visible_version_in_bitmaps(
      uint64_t core, uint64_t tx) {
    auto [second_core, first_core] =
        find_the_two_largest_among_or_less_than(core_bitmap_, core);

    if (first_core == -1) {
      assert(first_core == -1 && second_core == -1);
      return {false, 0, 0};
    }

    int first_core_pos = count_prefix_sum(first_core);
    assert(first_core_pos < (int)tx_bitmaps_.size());

    if (first_core == (int)core) {
      int first_tx =
          find_the_largest_among_less_than(tx_bitmaps_[first_core_pos], tx);

      if (first_tx != -1) {
        return {true, first_core, first_tx};
      }

      if (second_core != -1) {
        int second_core_pos = count_prefix_sum(second_core);
        assert(second_core_pos < first_core_pos);
        return {true, second_core,
                find_the_largest(tx_bitmaps_[second_core_pos])};
      }

      return {false, 0, 0};
    }

    assert(0 <= first_core && (uint64_t)first_core < core);
    return {true, first_core, find_the_largest(tx_bitmaps_[first_core_pos])};
  }

  Version *create_pending_version(Stat &stat) {
    Version *version = new Version;
    version->rec = nullptr;  // TODO
    version->deleted = false;
    version->status = Version::VersionStatus::PENDING;
    stat.increment(Stat::MeasureType::Create);
    return version;
  }
};