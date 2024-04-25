#include <unistd.h>

#include <bitset>
#include <string>
#include <thread>
#include <vector>

#include "benchmarks/ycsb/include/config.hpp"
#include "benchmarks/ycsb/include/tx_runner.hpp"
#include "benchmarks/ycsb/include/tx_utils.hpp"
#include "indexes/masstree.hpp"
#include "protocols/caracal/include/buffer.hpp"
#include "protocols/caracal/include/caracal.hpp"
#include "protocols/caracal/include/operation_set.hpp"
#include "protocols/caracal/include/value.hpp"
#include "protocols/caracal/ycsb/initializer.hpp"
#include "protocols/caracal/ycsb/transaction.hpp"
#include "protocols/common/timestamp_manager.hpp"
#include "protocols/ycsb_common/definitions.hpp"
#include "protocols/ycsb_common/epoch_controller.hpp"
#include "protocols/ycsb_common/make_transactions.hpp"
#include "protocols/ycsb_common/rendezvous_barrier.hpp"
#include "utils/logger.hpp"
#include "utils/numa.hpp"
#include "utils/utils.hpp"

volatile mrcu_epoch_type active_epoch = 1;
volatile std::uint64_t globalepoch = 1;
volatile bool recovering = false;

#ifdef PAYLOAD_SIZE
using Record = Payload<PAYLOAD_SIZE>;
#else
#define PAYLOAD_SIZE 1024
using Record = Payload<PAYLOAD_SIZE>;
#endif

template <typename Protocol>
void do_initialization_phase(uint64_t worker_id, uint64_t head_in_the_epoch,
                             Protocol& caracal,
                             std::vector<OperationSet>& txs) {
  for (uint64_t i = 0; i < NUM_TXS_IN_ONE_EPOCH_IN_ONE_CORE; i++) {
    caracal.txid_ = (i * 64) + worker_id;  // round-robin assignment

    std::vector<Operation>& w_set_ =
        txs[head_in_the_epoch + (i * 64) + worker_id]
            .w_set_;  // round-robin assignment

    for (size_t j = 0; j < w_set_.size(); j++) {
      caracal.append_pending_version(get_id<Record>(), w_set_[j].index_,
                                     w_set_[j].pending_);
    }
    caracal.terminate_transaction();
  }
}

template <typename Protocol>
void do_execution_phase(uint64_t worker_id, uint64_t head_in_the_epoch,
                        Protocol& caracal, std::vector<OperationSet>& txs) {
  for (uint64_t i = 0; i < NUM_TXS_IN_ONE_EPOCH_IN_ONE_CORE; i++) {
    assert(i < txs.size());
    caracal.txid_ = (i * 64) + worker_id;  // round-robin assignment
    std::vector<Operation>& rw_set =
        txs[head_in_the_epoch + (i * 64) + worker_id]
            .rw_set_;  // round-robin assignment
    for (size_t j = 0; j < rw_set.size(); j++) {
      if (rw_set[j].ope_ == Operation::Ope::Read) {
        caracal.read(get_id<Record>(), rw_set[j].index_);
      } else if (rw_set[j].ope_ == Operation::Ope::Update) {
        if (rw_set[j].pending_) {  // TODO: txθ: w(1)...w(1)
          caracal.write(get_id<Record>(), rw_set[j].pending_);
        }
      }
    }
  }
}

template <typename Protocol>
void run_tx(RendezvousBarrier& exp, RendezvousBarrier& exec_phase_start,
            RendezvousBarrier& exec_phase_end,
            [[maybe_unused]] ThreadLocalData& t_data, uint32_t worker_id,
            [[maybe_unused]] TimeStampManager<Protocol>& tsm,
            RowBufferController& rrc, std::vector<OperationSet>& txs) {
  uint64_t init_total = 0, exec_total = 0;
  uint64_t init_start, init_end, exec_start, exec_end;

  [[maybe_unused]] Config& c = get_mutable_config();

  // Pre-Initialization Phase
  // Core Assignment -> caracal: sequential v
  pid_t tid = gettid();
  Numa numa(tid, worker_id);
  assert(numa.cpu_ == worker_id);  // TODO: 削除
  t_data.stat.record(Stat::MeasureType::Core, numa.cpu_);
  t_data.stat.record(Stat::MeasureType::Node, numa.node_);

  Protocol caracal(numa.cpu_, worker_id, rrc);

  exp.send_ready_and_wait_start();  // rendezvous barrier

  uint64_t epoch = 1;
  while (epoch <= NUM_EPOCH) {
    uint64_t head_in_the_epoch = (epoch - 1) * NUM_TXS_IN_ONE_EPOCH;

    caracal.epoch_ = epoch;

    init_start = rdtscp();
    do_initialization_phase(worker_id, head_in_the_epoch, caracal, txs);
    /*
       At the end of the initialization phase, each core batch-appends all
       versions in its own slice of each row buffer to the version arrays of the
       corresponding rows. A core’s batch-append operations can occur while
       other cores are still run-ning the initialization phase and possibly
       allocating new row buffers.

       If all cores finish initialization at exactly the same time, they may
       contend on some rows during batch-append. If this happens, Caracal will
       delay batch-appending the contending row and process other rows first.
    */
    caracal.finalize_batch_append();
    init_end = rdtscp();
    init_total = init_total + (init_end - init_start);

    exec_phase_start.send_ready_and_wait_start();  // rendezvous barrier

    exec_start = rdtscp();
    do_execution_phase(worker_id, head_in_the_epoch, caracal, txs);
    exec_end = rdtscp();
    exec_total = exec_total + (exec_end - exec_start);

    exec_phase_end.send_ready_and_wait_start();  // rendezvous barrier

    epoch++;
  }

  t_data.stat.record(Stat::MeasureType::InitializationTime, init_total);
  t_data.stat.record(Stat::MeasureType::ExecutionTime, exec_total);
}

int main(int argc, const char* argv[]) {
  if (argc != 9) {
    printf(
        "seconds protocol workload_type(A,B,C,F) num_records num_threads skew "
        "reps_per_txn exp_id\n");
    exit(1);
  }

  [[maybe_unused]] int seconds = std::stoi(argv[1], nullptr, 10);
  std::string protocol = argv[2];
  std::string workload_type = argv[3];
  uint64_t num_records = static_cast<uint64_t>(std::stoi(argv[4], nullptr, 10));
  int num_threads = std::stoi(argv[5], nullptr, 10);
  double skew = std::stod(argv[6]);
  int reps = std::stoi(argv[7], nullptr, 10);
  [[maybe_unused]] int exp_id = std::stoi(argv[8], nullptr, 10);

  assert(seconds > 0);

  Config& c = get_mutable_config();
  c.set_protocol(protocol);
  c.set_workload_type(workload_type);
  c.set_num_records(num_records);
  c.set_num_threads(num_threads);
  c.set_contention(skew);
  c.set_reps_per_txn(reps);

  printf("Loading all tables with %lu record(s) each with %u bytes\n",
         num_records, PAYLOAD_SIZE);

  using Index = MasstreeIndexes<Value<Version>>;
  using Protocol = Caracal<Index>;

  Initializer<Index>::load_all_tables<Record>();  // make database
  printf("Loaded\n");

  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  TimeStampManager<Protocol> tsm(num_threads, 5);

  std::vector<ThreadLocalData> t_data(num_threads);

  RowBufferController rrc;

  RendezvousBarrier exp(num_threads), exec_phase_start(num_threads),
      exec_phase_end(num_threads);

  std::vector<OperationSet> txs(NUM_ALL_TXS);
  make_transactions(txs);

  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(run_tx<Protocol>, std::ref(exp),
                         std::ref(exec_phase_start), std::ref(exec_phase_end),
                         std::ref(t_data[i]), i, std::ref(tsm), std::ref(rrc),
                         std::ref(txs));
  }

  epoch_controller(exp, exec_phase_start, exec_phase_end, NUM_EPOCH);
  for (int i = 0; i < num_threads; i++) {
    threads[i].join();
  }

  Stat stat;
  std::string filepath = stat.prepare_result_file();
  for (uint64_t i = 0; i < t_data.size(); i++) {
    t_data[i].stat.log(filepath);
  };
}
