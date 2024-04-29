#pragma once

class RendezvousBarrierVariable {
  public:
    alignas(64) bool start_ =
        false; // parent to children: true when all threads is ready
    alignas(64) int ready_; // children to parent: decremented when the thread
                            // is ready
    int num_children_;

    void set_num_children_before_experiment(int num_children) {
        num_children_ = num_children;
        ready_ = num_children;
    }

    // called from parent
    void wait_all_children_ready() {
        while (0 < __atomic_load_n(&ready_, __ATOMIC_SEQ_CST)) {
            // spin
            asm volatile("pause" : : : "memory"); // equivalent to "rep; nop"
        }
    }
    // called from parent
    void send_start_to_all_children() {
        __atomic_store_n(&start_, true, __ATOMIC_SEQ_CST);
    }
    // called from parent
    void initialize() {
        __atomic_store_n(&start_, false, __ATOMIC_SEQ_CST);
        __atomic_store_n(&ready_, num_children_, __ATOMIC_SEQ_CST);
    }

    // called from children
    void send_ready_to_parent() {
        __atomic_sub_fetch(&ready_, 1, __ATOMIC_SEQ_CST);
    }
    // called from children
    void wait_start() {
        while (!__atomic_load_n(&start_, __ATOMIC_SEQ_CST)) {
            // spin
            asm volatile("pause" : : : "memory"); // equivalent to "rep; nop"
        }
    }
};

class RendezvousBarrier {
  public:
    enum BarrierType : int { StartExp, StartExecPhase, StartNewEpoc, Size };

    RendezvousBarrier(int num_children) {
        for (int i = 0; i < BarrierType::Size; i++) {
            variables_[i].set_num_children_before_experiment(num_children);
        }
    };

    // called from parent
    void wait_all_children_and_send_start(BarrierType type) {
        variables_[type].wait_all_children_ready();
        if (type == BarrierType::StartExecPhase) {
            variables_[BarrierType::StartNewEpoc].initialize();
        } else if (type == BarrierType::StartNewEpoc) {
            variables_[BarrierType::StartExecPhase].initialize();
        }
        variables_[type].send_start_to_all_children();
    }

    // called from children
    void send_ready_and_wait_start(BarrierType type) {
        variables_[type].send_ready_to_parent();
        variables_[type].wait_start();
    }

  private:
    RendezvousBarrierVariable variables_[BarrierType::Size];
};
