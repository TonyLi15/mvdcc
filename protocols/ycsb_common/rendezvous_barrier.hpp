#pragma once

class RendezvousBarrier {
 private:
  alignas(64) bool start_ =
      false;               // parent to children: true when all threads is ready
  alignas(64) int ready_;  // children to parent: decremented when the thread is
                           // ready
  alignas(64) int num_threads_;

  void send_ready_to_parent() {
    __atomic_sub_fetch(&ready_, 1, __ATOMIC_SEQ_CST);
  }
  void wait_start() {
    while (!__atomic_load_n(&start_, __ATOMIC_SEQ_CST)) {
      // spin
      asm volatile("pause" : : : "memory");  // equivalent to "rep; nop"
    }
  }
  // called from main thread
  void wait_all_children_ready() {
    while (0 < __atomic_load_n(&ready_, __ATOMIC_SEQ_CST)) {
      // spin
      asm volatile("pause" : : : "memory");  // equivalent to "rep; nop"
    }
  }
  // called from main thread
  void send_start_to_all_children() {
    __atomic_store_n(&start_, true, __ATOMIC_SEQ_CST);
  }

 public:
  void wait_all_children() { wait_all_children_ready(); }
  void send_start() { send_start_to_all_children(); }
  void initialize() {
    __atomic_store_n(&start_, false, __ATOMIC_SEQ_CST);
    __atomic_store_n(&ready_, num_threads_, __ATOMIC_SEQ_CST);
  }

  void send_ready_and_wait_start() {
    send_ready_to_parent();
    wait_start();
  }

  RendezvousBarrier(int num_threads)
      : ready_(num_threads), num_threads_(num_threads){};
};
