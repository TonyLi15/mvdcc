#pragma once

#include "benchmarks/ycsb/include/config.hpp"
#include "benchmarks/ycsb/include/record_key.hpp"
#include "benchmarks/ycsb/include/record_layout.hpp"
#include "protocols/common/memory_allocator.hpp"
#include "protocols/common/schema.hpp"
#include "protocols/common/tidword.hpp"
#include "protocols/ycsb_common/record_misc.hpp"
#include "utils/numa.hpp"
#include "utils/utils.hpp"

template <typename Index> class Initializer {
  private:
    using Key = typename Index::Key;
    using Value = typename Index::Value;

    static void insert_into_index(TableID table_id, Key key, void *rec,
                                  void *rec2) {
        // Value *val = reinterpret_cast<Value *>(
        //     MemoryAllocator::aligned_allocate(sizeof(Value)));
        // Version *epoch_1_version = reinterpret_cast<Version *>(
        //     MemoryAllocator::aligned_allocate(sizeof(Version)));
        Value *val = new Value;

        Version *epoch_1_version = new Version;
        val->initialize();
        epoch_1_version->rec = rec;
        epoch_1_version->status = Version::VersionStatus::STABLE;
        val->global_array_.append(epoch_1_version, -1);

        // Version *epoch_minus_1_version = reinterpret_cast<Version *>(
        //     MemoryAllocator::aligned_allocate(sizeof(Version)));
        Version *epoch_minus_1_version = new Version;
        epoch_minus_1_version->rec = rec2;
        epoch_minus_1_version->status = Version::VersionStatus::STABLE;
        val->master_ = epoch_minus_1_version;

        Index::get_index().insert(table_id, key, val);
    }

  public:
    template <typename Record> static void load_all_tables() {
        Schema &sch = Schema::get_mutable_schema();
        sch.set_record_size(get_id<Record>(), sizeof(Record));

        const Config &c = get_config();

        pid_t tid = gettid(); // fetch the thread's tid
        Numa numa(tid, 0);    // move to the designated core
        std::cout << "database is in node" << numa.node_ << std::endl;

        for (uint64_t key = 0; key < c.get_num_records(); key++) {
            // void *rec =
            //     new (MemoryAllocator::allocate(sizeof(Record))) Record();
            // void *rec2 =
            //     new (MemoryAllocator::allocate(sizeof(Record))) Record();
            void *rec = reinterpret_cast<void *>(new Record());
            void *rec2 = reinterpret_cast<void *>(new Record());
            insert_into_index(get_id<Record>(), key, rec, rec2);
        }
    }
};