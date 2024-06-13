#pragma once

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <stdlib.h>    // exit
#include <string.h>    // memset
#include <sys/ioctl.h> // ioctl
#include <sys/types.h> // pid_t
#include <unistd.h>    // syscall

#include <cstdint>

class Perf {
  private:
    struct read_format {
        uint64_t nr; /* The number of events */
        struct {
            uint64_t value; /* The value of the event */
        } values[2];
    };

    int fd_leader_;
    int fd_member_;
    int perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu,
                        int group_fd, unsigned long flags) {
        return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd,
                       flags);
    }

    void enable() {
        ioctl(fd_leader_, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    }

  public:
    class Output {
      public:
        uint64_t leader_;
        uint64_t member_;
    };

    Perf(const int cpu, pid_t tid) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.size = sizeof(struct perf_event_attr);

        pe.type = PERF_TYPE_HARDWARE;               // perf leader
        pe.config = PERF_COUNT_HW_CACHE_REFERENCES; // perf leader
        // pe.config = 0x2d3;  // mem_load_l3_miss_retired.remote_dram
        pe.type = PERF_TYPE_HW_CACHE; // perf leader
        pe.config = PERF_COUNT_HW_CACHE_NODE |
                    (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
                    (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16); // perf leader

        pe.read_format = PERF_FORMAT_GROUP;
        pe.disabled = 1;
        // pe.exclude_user = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;
        pe.exclude_idle = 1;
        pe.exclude_guest = 1;
        fd_leader_ = perf_event_open(&pe, tid, cpu, -1, 0);
        if (fd_leader_ == -1) {
            assert(false);
            exit(EXIT_FAILURE);
        }

        // pe.type = PERF_TYPE_HARDWARE;            // perf member
        // pe.config = PERF_COUNT_HW_CACHE_MISSES;  // perf member
        pe.type = PERF_TYPE_HW_CACHE; // perf member
        pe.config = PERF_COUNT_HW_CACHE_NODE |
                    (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
                    (PERF_COUNT_HW_CACHE_RESULT_MISS << 16); // perf member

        fd_member_ = perf_event_open(&pe, tid, cpu, fd_leader_, 0);
        if (fd_member_ == -1) {
            assert(false);
            exit(EXIT_FAILURE);
        }

        int ioctl_res =
            ioctl(fd_leader_, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
        if (ioctl_res == -1) {
            assert(false);
            exit(EXIT_FAILURE);
        }

        enable();
    }
    ~Perf() {
        ioctl(fd_leader_, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
        close(fd_leader_);
        close(fd_member_);
    }
    void perf_read(Output &out) {
        struct read_format format;
        ssize_t res = read(fd_leader_, &format, sizeof(struct read_format));
        if (res == -1)
            assert(false);
        out.leader_ = format.values[0].value;
        out.member_ = format.values[1].value;
    }
};
