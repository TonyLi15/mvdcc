#!/usr/bin/env python3

import os
from sys import executable

import pandas as pd
import numpy as np
import datetime


import module.plot as plot
import module.setting as setting

# EXECUTE THIS SCRIPT IN BASE DIRECTORY!!!

# Set to 10 when running for paper, 1 for just test
NUM_EXPERIMENTS_PER_SETUP = 10
NUM_SECONDS = 1  
VARYING_TYPE = "contention"  

x_label = {
    "num_threads": "#thread",
    "reps": "#operations",
    "contention": "skew",
    "MAX_SLOTS_OF_PER_CORE_BUFFER": "#slots of buffer",
    "NUM_TXS_IN_ONE_EPOCH": "NUM_TXS_IN_ONE_EPOCH"
}

# 1 Protocol Experiments
# protocols = ["caracal"]
# protocols = ["serval"]
# protocols = ["cheetah"]

# 2 Protocols Experiments
# protocols = ["caracal", "serval"]
# protocols = ["caracal", "cheetah"]
# protocols = ["serval", "cheetah"]

# 3 Protocols Experiments
protocols = ["caracal", "serval", "cheetah"]


# Build Type -> "Debug" when testing, "Release" when doing experiments used in Paper
CMAKE_BUILD_TYPE = "Release"


def add_options_to_protocol(protocol, bcbu, rc):
    options = [protocol]
    if bcbu:
        options.append("BCBU")
    if rc:
        options.append("RC")
    return "_".join(options)

def gen_setups():
    payloads = [4]

    # =========== for caracal ===========
    buffer_slots = [255] # 255
    # ===================================

    # =========== for serval (IGNORE THIS PART)===========
    bcbus = [0] # batch_core_bitmap_updates: 0 is good
    rcs = [0] # reference counter (read counter)
    # ===================================

    # =========== common ===========
    txs_in_epochs = [4096] # DO NOT CHANGE
    # ===================================

    # =========== workload parameters ===========
    workloads = ["X"] # 0:100 // Write Only 
    # workloads = ["A"] # 50:50 // Update heavy
    # workloads = ["C"] # 100:0 // Read Only
    # workloads = ["B"] # 95:5 // Read heavy
    # workloads = ["W90"] # 10:90 // Write intensive
    # workloads = ["W80"] # 20:80 // Write moderate intensive

    # skews = [0.7, 0.8, 0.85, 0.9, 0.95, 0.99] # 0.0 - 0.99
    # skews = [0.5, 0.9, 0.99] # 0.0 - 0.99
    # skews = [0.7, 0.8, 0.9, 0.99]
    skews = [0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.99] # 0.0 - 0.99, skip = 0.01
    # skews = [0.5]
    # skews = [0.85, 0.86, 0.87, 0.88, 0.89, 0.9, 0.91, 0.92, 0.93, 0.94, 0.95, 0.96, 0.97, 0.98, 0.99] # high contention, 0.85 - 0.99, skip = 0.01

    repss = [10] # DO NOT CHANGE
    records = [10000000] # DO NOT CHANGE
    threads = [64] # DO NOT CHANGE
    # ============================================

    return [
        [
            [protocol, str(payload), str(buffer_slot), str(txs_in_epoch), str(bcbu), str(rc)],
            [
                add_options_to_protocol(protocol, bcbu, rc),
                workload,
                str(record),
                str(thread),
                str(skew),
                str(reps),
            ],
        ]
        for protocol in protocols
        for payload in payloads
        for buffer_slot in buffer_slots
        for txs_in_epoch in txs_in_epochs
        for bcbu in bcbus
        for rc in rcs
        for workload in workloads
        for record in records
        for thread in threads
        for skew in skews
        for reps in repss
    ]


def build():
    if not os.path.exists("./build"):
        os.mkdir("./build")  # create build
    os.chdir("./build")
    if not os.path.exists("./log"):
        os.mkdir("./log")  # compile logs
    for setup in gen_setups():
        [[protocol, payload, buffer_slot, txs_in_epoch, bcbu, rc], _] = setup
        print("Compiling " + " PAYLOAD_SIZE=" + payload + " MAX_SLOTS_OF_PER_CORE_BUFFER=" + buffer_slot + " NUM_TXS_IN_ONE_EPOCH=" + txs_in_epoch + " BCBU=" + bcbu, " RC=" + rc)
        logfile = "_PAYLOAD_SIZE" + payload + "_MAX_SLOTS_OF_PER_CORE_BUFFER" + buffer_slot + ".compile_log"
        os.system(
            "cmake .. -DLOG_LEVEL=0 -DCMAKE_BUILD_TYPE="
            + CMAKE_BUILD_TYPE +
            " -DBENCHMARK=YCSB -DCC_ALG="
            + protocol.upper()
            + " -DPAYLOAD_SIZE="
            + payload
            + " -DMAX_SLOTS_OF_PER_CORE_BUFFER="
            + buffer_slot
            + " -DNUM_TXS_IN_ONE_EPOCH="
            + txs_in_epoch
            + " -DBCBU="
            + bcbu
            + " -DRC="
            + rc
            + " > ./log/"
            + "compile_"
            + logfile
            + " 2>&1"
        )
        ret = os.system("make -j > ./log/" + logfile + " 2>&1")
        if ret != 0:
            print("Error. Stopping")
            exit(0)
    os.chdir("../")  # go back to base directory


def run_all():
    os.chdir("./build/bin")  # move to bin
    if not os.path.exists("./res"):
        os.mkdir("./res")  # create result directory inside bin
        os.mkdir("./res/tmp")
    for setup in gen_setups():
        [
            [protocol, payload, buffer_slot, txs_in_epoch, bcbu, rc],
            args,
        ] = setup
        title = "ycsb" + payload + "_" + buffer_slot + "_" + txs_in_epoch + "_" + bcbu + "_" + rc + "_" + protocol

        print("[{}: {}]".format(title, " ".join([str(NUM_SECONDS), *args])))

        for exp_id in range(NUM_EXPERIMENTS_PER_SETUP):
            dt_now = datetime.datetime.now()
            print(" Trial:" + str(exp_id))
            ret = os.system(
                # "numactl --interleave=all ./" # 多分これするとregionが壊れる
                "./"
                + title
                + " "
                + " ".join([str(NUM_SECONDS), *args, str(exp_id)])
                + " > ./res/tmp/"
                + str(dt_now.isoformat())
                + " 2>&1"
            )
            if ret != 0:
                print("Error. Stopping")
                exit(0)
    ret = os.system(
        "cat ./res/*.csv > ./res/result.csv; cat ./res/header > ./res/concat.csv; cat ./res/result.csv >> ./res/concat.csv"
    )
    if ret != 0:
        print("Error. Stopping")
        exit(0)
    os.chdir("../../")  # back to base directory


def plot_all():
    path = "build/bin/res"
    # plot throughput
    os.chdir(path)  # move to result file

    header = pd.read_csv("header", sep=",").columns.tolist()
    compile_param = pd.read_csv("compile_params", sep=",").columns.tolist()
    runtime_param = pd.read_csv("runtime_params", sep=",").columns.tolist()
    df = pd.read_csv("result.csv", sep=",", names=header)
    print(df.info())
    runtime_protocols = df['protocol'].unique()

    dfs = {}
    grouped_dfs = {}
    for protocol in runtime_protocols:
        protocol_df = df[df["protocol"] == protocol]
        protocol_grouped_df = protocol_df.groupby(compile_param + runtime_param, as_index=False).sum()
        for column in protocol_grouped_df.columns:
            if column in ["Create","Delete","TotalTime","InitializationTime","ExecutionTime","Sync1Time","Sync2Time","WaitInInitialization","WaitInExecution","WaitInGC","PerfLeader","PerfMember"]:
                protocol_grouped_df[column] = protocol_grouped_df[column] / 64 / NUM_EXPERIMENTS_PER_SETUP
        grouped_dfs[protocol] = protocol_grouped_df
        dfs[protocol] = protocol_df


    if not os.path.exists("./plots"):
        os.mkdir("./plots")  # create plot directory inside res
    os.chdir("./plots")
    
    plot_params = ["Create","Delete","TotalTime","InitializationTime","ExecutionTime","Sync1Time","Sync2Time","WaitInInitialization","WaitInExecution","WaitInGC","PerfLeader","PerfMember"]
    my_plot = plot.Plot(
        VARYING_TYPE,
        x_label,
        runtime_protocols,
        plot_params,  # change
    )  # change

    my_plot.plot_all_param_all_protocol(grouped_dfs)
    my_plot.histogram_of_init_and_exec_phase(grouped_dfs, 0.9)

    # my_plot.plot_all_param_per_core("serval", dfs["serval"])
    # my_plot.plot_all_param_per_core("serval_rc", dfs["serval_rc"])
    # my_plot.plot_all_param_per_core("serval_rc_bbu", dfs["serval_rc_bbu"])
    # my_plot.plot_all_param_per_core("serval_BCBU", dfs["serval_BCBU"])
    # my_plot.plot_all_param_per_core("caracal", dfs["caracal"])
    # my_plot.plot_all_param_per_core("serval", dfs["serval"])

    # my_plot.plot_cache_hit_rate(grouped_dfs)
    # my_plot.plot_all_param("caracal", grouped_dfs["caracal"])

    os.chdir("../../../../")  # go back to base directory


if __name__ == "__main__":
    build()
    run_all()
    plot_all()
