#include <unistd.h>

#include <bitset>
#include <string>
#include <thread>
#include <vector>

#include "benchmarks/ycsb/include/config.hpp"
#include "benchmarks/ycsb/include/tx_runner.hpp"
#include "benchmarks/ycsb/include/tx_utils.hpp"
#include "indexes/masstree.hpp"
#include "protocols/serval/include/operation_set.hpp"
#include "protocols/serval/include/region.hpp"
#include "protocols/serval/include/serval.hpp"
#include "protocols/serval/include/value.hpp"
#include "protocols/serval/ycsb/initializer.hpp"
#include "protocols/serval/ycsb/transaction.hpp"
#include "protocols/ycsb_common/definitions.hpp"
#include "protocols/ycsb_common/rendezvous_barrier.hpp"
#include "utils/logger.hpp"
#include "utils/numa.hpp"
#include "utils/perf.hpp"
#include "utils/tsc.hpp"
#include "utils/utils.hpp"

volatile mrcu_epoch_type active_epoch = 1;
volatile std::uint64_t globalepoch = 1;
volatile bool recovering = false;

template <typename Protocol>
void do_initialization_phase(uint64_t worker_id, uint64_t head_in_the_epoch,
                             Protocol &serval, std::vector<OperationSet> &txs) {
    for (uint64_t i = 0; i < NUM_TXS_IN_ONE_EPOCH_IN_ONE_CORE; i++) {
        serval.txid_ = (worker_id * 64) + i; // sequential assignment
        serval.core_ = worker_id;
        std::vector<Operation *> &w_set_ =
            txs[head_in_the_epoch + (worker_id * 64) + i]
                .w_set_; // sequential assignment

        for (size_t j = 0; j < w_set_.size(); j++) {
            serval.append_pending_version(get_id<Record>(), w_set_[j]->index_,
                                          w_set_[j]->pending_);
        }
        serval.terminate_transaction();
    }
}

template <typename Protocol>
void do_execution_phase(uint64_t worker_id, uint64_t head_in_the_epoch,
                        Protocol &serval, std::vector<OperationSet> &txs) {
    for (uint64_t i = 0; i < NUM_TXS_IN_ONE_EPOCH_IN_ONE_CORE; i++) {
        assert(i < txs.size());
        serval.txid_ = (i * 64) + worker_id; // round-robin assignment
        serval.core_ = i;
        std::vector<Operation *> &rw_set =
            txs[head_in_the_epoch + (i * 64) + worker_id]
                .rw_set_; // round-robin assignment
        for (size_t j = 0; j < rw_set.size(); j++) {
            if (rw_set[j]->ope_ == Operation::Ope::Read) {
                serval.read(get_id<Record>(), rw_set[j]->index_);
            } else if (rw_set[j]->ope_ == Operation::Ope::Update) {
                if (rw_set[j]->pending_) { // TODO: txθ: w(1)...w(1)
                    serval.write(get_id<Record>(), rw_set[j]->pending_);
                }
            }
        }
    }
}

void rendezvous_barrier_to_start(RendezvousBarrier::BarrierType type,
                                 RendezvousBarrier &rend, uint32_t worker_id) {
    if (worker_id == 63) {
        // do parent work
        rend.wait_all_children_and_send_start(type);
    } else {
        // do children work
        rend.send_ready_and_wait_start(type);
    }
}

template <typename Protocol>
void run_tx(RendezvousBarrier &rend, [[maybe_unused]] ThreadLocalData &t_data,
            uint32_t worker_id, RowRegionController &rrc,
            std::vector<OperationSet> &txs) {
    uint64_t init_total = 0, exec_total = 0;
    uint64_t init_start, init_end, exec_start, exec_end;
    [[maybe_unused]] Config &c = get_mutable_config();

    // Pre-Initialization Phase
    pid_t tid = gettid();
    Numa numa(tid, worker_id);
    assert(numa.cpu_ == worker_id); // TODO: 削除
    t_data.stat.record(Stat::MeasureType::Core, numa.cpu_);
    t_data.stat.record(Stat::MeasureType::Node, numa.node_);

    Protocol serval(numa.cpu_, worker_id, rrc, t_data.stat);

    // Perf perf(worker_id, tid);
    // Perf::Output perf_start, perf_end;

    rendezvous_barrier_to_start(RendezvousBarrier::BarrierType::StartExp, rend,
                                worker_id);
    // uint64_t exp_start = worker_id == 0 ? rdtscp() : 0;
    uint64_t exp_start = rdtscp();
    // perf.perf_read(perf_start);

    uint64_t epoch = 1;
    while (epoch <= NUM_EPOCH) {
        serval.epoch_ = epoch;

        uint64_t head_in_the_epoch = (epoch - 1) * NUM_TXS_IN_ONE_EPOCH;

        init_start = rdtscp();
        do_initialization_phase(worker_id, head_in_the_epoch, serval, txs);
#if BATCH_CORE_BITMAP_UPDATE
        serval.batch_core_bitmap_update();
#endif
        init_end = rdtscp();
        init_total = init_total + (init_end - init_start);

        rendezvous_barrier_to_start(
            RendezvousBarrier::BarrierType::StartExecPhase, rend, worker_id);

        exec_start = rdtscp();
        do_execution_phase(worker_id, head_in_the_epoch, serval, txs);
        exec_end = rdtscp();
        exec_total = exec_total + (exec_end - exec_start);

        rendezvous_barrier_to_start(
            RendezvousBarrier::BarrierType::StartNewEpoc, rend, worker_id);
        epoch++; // new epoch start
    }
    // uint64_t exp_end = worker_id == 0 ? rdtscp() : 0;
    uint64_t exp_end = rdtscp();
    // perf.perf_read(perf_end);

    t_data.stat.record(Stat::MeasureType::TotalTime, exp_end - exp_start);
    t_data.stat.record(Stat::MeasureType::InitializationTime, init_total);
    t_data.stat.record(Stat::MeasureType::ExecutionTime, exec_total);

    // t_data.stat.record(Stat::MeasureType::PerfLeader,
    //                    perf_end.leader_ - perf_start.leader_);
    // t_data.stat.record(Stat::MeasureType::PerfMember,
    //                    perf_end.member_ - perf_start.member_);
}

bool has_write(OperationSet &tx, uint64_t key) {
    for (auto &write : tx.w_set_) {
        if (write->index_ == key)
            return true;
    }
    return false;
}
// void print_database([[maybe_unused]] std::vector<OperationSet> &txs) {
//     using Index = MasstreeIndexes<Value<Version>>;
//     [[maybe_unused]] Config &c = get_mutable_config();
//     for (uint64_t key = 0; key < c.get_num_records(); key++) {
//         Index &idx = Index::get_index();
//         Value<Version> *val;
//         typename Index::Result res = idx.find(
//             get_id<Record>(), key, val); // find corresponding index in
//             masstree
//         if (res == Index::Result::NOT_FOUND)
//             return;
//         if (val->global_array_.initialized) {
//             for (size_t i = 0; i < val->global_array_.ids_slots_.size();
//             i++) {
//                 assert(0 <= val->global_array_.ids_slots_[i].first);
//                 assert(val->global_array_.ids_slots_[i].second->status ==
//                        Version::VersionStatus::STABLE);
//                 assert(has_write(txs[val->global_array_.ids_slots_[i].first],
//                                  key));
//             }
//             if (val->core_bitmap_) {
//                 for (size_t core = 0; core < 64; core++) {
//                     PerCoreVersionArray *array =
//                         val->row_region_->arrays_[core];
//                     assert(array->length() == (int)array->slots_.size());
//                     assert(array->length() ==
//                     (int)array->debug_ids_.size()); for (size_t i = 0; i
//                     < array->slots_.size(); i++) {
//                         assert(array->slots_[i]->status ==
//                                Version::VersionStatus::STABLE);
//                         assert(has_write(txs[core * 64 +
//                         array->debug_ids_[i]],
//                                          key));
//                     }
//                 }
//             }

//         } else {
//             assert(val->global_array_.ids_slots_.size() == 1);
//         }

//         // std::cout << key << ": ";
//         // val->global_array_.print();
//     }
// }

int main(int argc, const char *argv[]) {
    if (argc != 9) {
        printf("seconds protocol workload_type(A,B,C,F) num_records "
               "num_threads skew "
               "reps_per_txn exp_id\n");
        exit(1);
    }

    [[maybe_unused]] int seconds = std::stoi(argv[1], nullptr, 10);
    std::string protocol = argv[2];
    std::string workload_type = argv[3];
    uint64_t num_records =
        static_cast<uint64_t>(std::stoi(argv[4], nullptr, 10));
    int num_threads = std::stoi(argv[5], nullptr, 10);
    double skew = std::stod(argv[6]);
    int reps = std::stoi(argv[7], nullptr, 10);
    [[maybe_unused]] int exp_id = std::stoi(argv[8], nullptr, 10);

    assert(seconds > 0);

    Config &c = get_mutable_config();
    c.set_protocol(protocol);
    c.set_workload_type(workload_type);
    c.set_num_records(num_records);
    c.set_num_threads(num_threads);
    c.set_contention(skew);
    c.set_reps_per_txn(reps);

    printf("Loading all tables with %lu record(s) each with %u bytes\n",
           num_records, PAYLOAD_SIZE);

    using Index = MasstreeIndexes<Value<Version>>;
    using Protocol = Serval<Index>;

    Initializer<Index>::load_all_tables<Record>();
    printf("Loaded\n");

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    std::vector<ThreadLocalData> t_data(num_threads);

    RowRegionController rrc;
    RendezvousBarrier rend(num_threads - 1);

    std::vector<OperationSet> txs(NUM_ALL_TXS);
    for (size_t i = 0; i < txs[0].rw_set_.size(); i++) {
        std::cout << txs[0].rw_set_[i]->index_ << std::endl;
    }

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(run_tx<Protocol>, std::ref(rend),
                             std::ref(t_data[i]), i, std::ref(rrc),
                             std::ref(txs));
    }
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    // print_database(txs);

    Stat stat;
    std::string filepath = stat.prepare_result_file();
    for (size_t i = 0; i < t_data.size(); i++) {
        t_data[i].stat.log(filepath);
    };
}
