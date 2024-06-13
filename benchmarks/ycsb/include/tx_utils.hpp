#pragma once

#include <filesystem> // TODO
#include <fstream>    // TODO
#include <string>     // TODO

#include "benchmarks/ycsb/include/config.hpp"
#include "protocols/ycsb_common/definitions.hpp"
#include "utils/utils.hpp"

enum Status {
    SUCCESS = 0,  // if all stages of transaction return Result::SUCCESS
    USER_ABORT,   // if rollback defined in the specification occurs (e.g. 1% of
                  // NewOrder Tx)
    SYSTEM_ABORT, // if any stage of a transaction returns Result::ABORT
    BUG // if any stage of a transaciton returns unexpected Result::FAIL
};

enum TxProfileID : uint8_t {
    READ_TX = 0,
    UPDATE_TX = 1,
    READMODIFYWRITE_TX = 2,
    MAX = 3
};

template <typename Record> class ReadTx;

template <typename Record> class ReadModifyWriteTx;

template <typename Record> class UpdateTx;

template <TxProfileID i> struct TxType;

template <> struct TxType<TxProfileID::READ_TX> {
    template <typename Record> using Profile = ReadTx<Record>;
};

template <> struct TxType<TxProfileID::READMODIFYWRITE_TX> {
    template <typename Record> using Profile = ReadModifyWriteTx<Record>;
};

template <> struct TxType<TxProfileID::UPDATE_TX> {
    template <typename Record> using Profile = UpdateTx<Record>;
};

template <TxProfileID i, typename Record>
using TxProfile = typename TxType<i>::template Profile<Record>;

struct Stat {
    enum MeasureType : int {
        Core,
        Node,
        TotalTime,
        InitializationTime,
        FinalizeInitializationTime,
        ExecutionTime,
        WaitInInitialization,
        WaitInExecution,
        PerfLeader,
        PerfMember,
        Size
    };

    std::vector<std::string> measure_type_name = {
        "Core",
        "Node",
        "TotalTime",
        "InitializationTime",
        "FinalizeInitializationTime",
        "ExecutionTime",
        "WaitInInitialization",
        "WaitInExecution",
        "PerfLeader",
        "PerfMember",
    };

    std::vector<std::string> compile_params_ = {
        std::to_string(PAYLOAD_SIZE),
        std::to_string(MAX_SLOTS_OF_PER_CORE_BUFFER),
        std::to_string(NUM_TXS_IN_ONE_EPOCH), std::to_string(CLOCKS_PER_US)};
    std::vector<std::string> compile_params_name = {
        "PAYLOAD_SIZE", "MAX_SLOTS_OF_PER_CORE_BUFFER", "NUM_TXS_IN_ONE_EPOCH",
        "CLOCKS_PER_US"};
    std::vector<std::string> get_runtime_params() {
        const Config &c = get_config();
        return {c.get_protocol(),
                std::to_string(c.get_num_records()),
                std::to_string(c.get_num_threads()),
                std::to_string(c.get_contention()),
                std::to_string(c.get_reps_per_txn()),
                std::to_string(c.get_read_propotion()),
                std::to_string(c.get_update_propotion())};
    }
    std::vector<std::string> runtime_params_name = {
        "protocol",     "num_records",    "num_threads",     "contention",
        "reps_per_txn", "read_propotion", "update_propotion"};

    std::string create_result_file_path() {
        std::filesystem::create_directory("res");
        auto now = std::chrono::system_clock::now();
        std::time_t end_time = std::chrono::system_clock::to_time_t(now);
        std::string filename = std::ctime(&end_time);
        return "res/" + filename + ".csv";
    }

    void create_runtime_file() {
        std::ofstream file;
        file.open("./res/runtime_params", std::ios::out);
        std::string line = "";
        for (size_t i = 0; i < runtime_params_name.size(); i++) {
            line.append(runtime_params_name[i] + ",");
        };
        line.pop_back();
        file << line << std::endl;
        file.close();
    };
    void create_compile_file() {
        std::ofstream file;
        file.open("./res/compile_params", std::ios::out);
        std::string line = "";
        for (size_t i = 0; i < compile_params_name.size(); i++) {
            line.append(compile_params_name[i] + ",");
        };
        line.pop_back();
        file << line << std::endl;
        file.close();
    };
    void create_header_file() {
        if (!std::filesystem::is_regular_file("header")) {
            std::ofstream file;
            file.open("./res/header", std::ios::out);
            std::string line = "";
            for (size_t i = 0; i < compile_params_name.size(); i++) {
                line.append(compile_params_name[i] + ",");
            };
            for (size_t i = 0; i < runtime_params_name.size(); i++) {
                line.append(runtime_params_name[i] + ",");
            };
            for (size_t i = 0; i < measure_type_name.size(); i++) {
                line.append(measure_type_name[i] + ",");
            };
            line.pop_back();
            file << line << std::endl;
            file.close();
        }
    };

    std::string prepare_result_file() {
        create_compile_file();
        create_runtime_file();
        create_header_file();
        return create_result_file_path();
    }

    uint64_t measures_[MeasureType::Size] = {0};
    // stat of one thread
    void log(std::string filepath) {
        std::ofstream file;
        file.open(filepath, std::ios::app);

        std::string line = "";
        for (size_t i = 0; i < compile_params_.size(); i++) {
            line.append(compile_params_[i] + ",");
        };
        std::vector<std::string> runtime_params = get_runtime_params();
        for (size_t i = 0; i < runtime_params.size(); i++) {
            line.append(runtime_params[i] + ",");
        };
        for (int i = 0; i < MeasureType::Size; i++) {
            line.append(std::to_string(measures_[i]) + ",");
        };
        line.pop_back();
        file << line << std::endl;
        file.close();
    }
    void record(MeasureType type, uint64_t n) { measures_[type] = n; }
    void increment(MeasureType type) { measures_[type]++; }
    void add(MeasureType type, uint64_t n) {
        measures_[type] = measures_[type] + n;
    }

    struct PerTxType {
        size_t num_commits = 0;
        size_t num_usr_aborts = 0;
        size_t num_sys_aborts = 0;

        void add(const PerTxType &rhs) {
            num_commits += rhs.num_commits;
            num_usr_aborts += rhs.num_usr_aborts;
            num_sys_aborts += rhs.num_sys_aborts;
        }
    };
    PerTxType &operator[](TxProfileID tx_type) { return per_type_[tx_type]; }
    const PerTxType &operator[](TxProfileID tx_type) const {
        return per_type_[tx_type];
    }

    void add(const Stat &rhs) {
        for (size_t i = 0; i < TxProfileID::MAX; i++) {
            per_type_[i].add(rhs.per_type_[i]);
        }
    }
    PerTxType aggregate_perf() const {
        PerTxType out;
        for (size_t i = 0; i < TxProfileID::MAX; i++) {
            out.add(per_type_[i]);
        }
        return out;
    }

  private:
    PerTxType per_type_[TxProfileID::MAX];
};

struct ThreadLocalData {
    alignas(64) Stat stat;
};

template <typename Transaction>
inline bool not_succeeded(Transaction &tx, typename Transaction::Result &res) {
    const Config &c = get_config();
    bool flag = c.get_random_abort_flag();
    // Randomized abort
    if (flag && res == Transaction::Result::SUCCESS && urand_int(1, 100) == 1) {
        res = Transaction::Result::ABORT;
    }
    if (res == Transaction::Result::ABORT) {
        tx.abort();
    }
    return res != Transaction::Result::SUCCESS;
}

template <typename Transaction> struct TxHelper {
    Transaction &tx_;
    Stat::PerTxType &per_type_;

    explicit TxHelper(Transaction &tx, Stat::PerTxType &per_type_)
        : tx_(tx), per_type_(per_type_) {}

    Status kill(typename Transaction::Result res) {
        switch (res) {
        case Transaction::Result::FAIL:
            return Status::BUG;
        case Transaction::Result::ABORT:
            per_type_.num_sys_aborts++;
            return Status::SYSTEM_ABORT;
        default:
            throw std::runtime_error("wrong Transaction::Result");
        }
    }

    Status commit() {
        if (tx_.commit()) {
            per_type_.num_commits++;
            return Status::SUCCESS;
        } else {
            per_type_.num_sys_aborts++;
            return Status::SYSTEM_ABORT;
        }
    }

    Status usr_abort() {
        per_type_.num_usr_aborts++;
        return Status::USER_ABORT;
    }
};
