#pragma once

#include <cstdint>

#include "protocols/ycsb_common/rendezvous_barrier.hpp"
#include "utils/tsc.hpp"

void epoch_controller(RendezvousBarrier& exp,
                      RendezvousBarrier& exec_phase_start,
                      RendezvousBarrier& exec_phase_end, uint64_t num_epoch) {
  uint64_t exp_start, exp_end;
  // rendezvous barrier
  exp.wait_all_children();
  exp.send_start();

  // expriment start
  exp_start = rdtscp();
  /*
  repeat the procedure below until all transactions is processed.

  one epoch:
  1. initialization phase
  2. synchronization
  3. execution phase
  4. synchronization
  */

  uint64_t epoch = 1;

  while (epoch <= num_epoch) {
    // initialization phase

    // execution phase
    exec_phase_start.wait_all_children();
    exec_phase_end.initialize();
    exec_phase_start.send_start();

    exec_phase_end.wait_all_children();
    exec_phase_start.initialize();
    exec_phase_end.send_start();  // new epoch start

    // epoch end
    epoch++;
  }

  exp_end = rdtscp();

  long double exec_sec = (exp_end - exp_start) / CLOCKS_PER_S;
  std::cout << "実行時間[MS]： " << (exp_end - exp_start) / CLOCKS_PER_MS
            << std::endl;
  std::cout << "実行時間[S]： " << (exp_end - exp_start) / CLOCKS_PER_S
            << std::endl;
  std::cout << "実行時間[S]： " << exec_sec << std::endl;
  std::cout << "処理したトランザクション数(NUM_ALL_TXS)： " << NUM_ALL_TXS
            << std::endl;
  std::cout << "TPS： " << (long double)NUM_ALL_TXS / exec_sec << std::endl;
  std::cout << "EPOCH数(NUM_EPOCH)： " << NUM_EPOCH << std::endl;
}
