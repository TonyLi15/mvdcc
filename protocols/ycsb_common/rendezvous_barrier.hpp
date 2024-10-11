#pragma once

class RendezvousBarrierVariable {
  public:
    enum BarrierType : int {
        BeforeExp,
        Exp,
        InitPhase,
        ExecPhase,
        NewEpoc,
        Size
    };

    alignas(64)
        BarrierType start_ = BarrierType::BeforeExp; // parent to children
    alignas(64) int ready_; // children to parent: decremented when the thread
                            // is ready

    int num_children_;

    RendezvousBarrierVariable(int num_children)
        : ready_(num_children), num_children_(num_children) {}

    // called from parent
    void wait_all_children_ready() {
        while (0 < __atomic_load_n(&ready_, __ATOMIC_SEQ_CST)) {
            // spin
            asm volatile("pause" : : : "memory"); // equivalent to "rep; nop"
        }
    }
    // called from parent
    void send_start_to_all_children(BarrierType type) {
        __atomic_store_n(&start_, type, __ATOMIC_SEQ_CST);
    }
    // called from parent
    void initialize() {
        __atomic_store_n(&ready_, num_children_, __ATOMIC_SEQ_CST);
    }

    // called from children
    void send_ready_to_parent() {
        __atomic_sub_fetch(&ready_, 1, __ATOMIC_SEQ_CST);
    }
    // called from children
    void wait_start(BarrierType type) {
        while (__atomic_load_n(&start_, __ATOMIC_SEQ_CST) != type) {
            // spin
            asm volatile("pause" : : : "memory"); // equivalent to "rep; nop"
        }
    }
};

class RendezvousBarrier {
  public:
    RendezvousBarrier(int num_children) : variable_(num_children){};

    // called from parent
    void wait_all_children_and_send_start(
        RendezvousBarrierVariable::BarrierType type) {
        variable_.wait_all_children_ready();
        variable_.initialize();
        variable_.send_start_to_all_children(type);
    }

    // called from children
    void
    send_ready_and_wait_start(RendezvousBarrierVariable::BarrierType type) {
        variable_.send_ready_to_parent();
        variable_.wait_start(type);
    }

  private:
    RendezvousBarrierVariable variable_;
};
