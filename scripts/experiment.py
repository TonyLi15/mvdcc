#!/usr/bin/env python3

import os
from sys import executable

import pandas as pd
import numpy as np
import datetime


import module.plot as plot
import module.setting as setting

# EXECUTE THIS SCRIPT IN BASE DIRECTORY!!!

# Following variables all used in plot:
NUM_EXPERIMENTS_PER_SETUP = 1 
NUM_SECONDS = 1
VARYING_TYPE = "contention"  

x_label = {
    "num_threads": "#thread",
    "reps": "#operations",
    "contention": "Skew",
}

protocol_id = {"CARACAL": 0, "SERVAL": 1}

protocols = ["caracal", "serval"]

CMAKE_BUILD_TYPE = "Release" # CHANGE TO "Release" when performing experiments for paper

def gen_setups():
    payloads = [4]
    workloads = ["X"] # Write Only
    records = [1000000] 
    threads = [64] 
    skews = [0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.99] # 0.0 - 0.99, skip = 0.1
    repss = [20]

    return [
        [
            [protocol, str(payload)],
            [
                protocol,
                workload,
                str(record),
                str(thread),
                str(skew),
                str(reps),
            ],
        ]
        for protocol in protocols
        for payload in payloads
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
        [[protocol, payload], _] = setup
        print("Compiling " + " PAYLOAD_SIZE=" + payload)
        logfile = "_PAYLOAD_SIZE_" + payload + ".compile_log"
        os.system(
            "cmake .. -DLOG_LEVEL=0 -DCMAKE_BUILD_TYPE="
            + CMAKE_BUILD_TYPE +
            " -DBENCHMARK=YCSB -DCC_ALG="
            + protocol.upper()
            + " -DPAYLOAD_SIZE="
            + payload
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
            [protocol, payload],
            args,
        ] = setup

        title = "ycsb" + payload + "_" + protocol

        print("[{}: {}]".format(title, " ".join([str(NUM_SECONDS), *args])))

        for exp_id in range(NUM_EXPERIMENTS_PER_SETUP):
            dt_now = datetime.datetime.now()
            print(" Trial:" + str(exp_id))
            ret = os.system(
                # "numactl --interleave=all ./" # Serval's region might be broken when performing this command
                "sudo ./"
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

    dfs = {}
    grouped_dfs = {}
    for protocol in protocols:
        protocol_df = df[df["protocol"] == protocol]
        protocol_grouped_df = protocol_df.groupby(runtime_param, as_index=False).sum()
        for column in protocol_grouped_df.columns:
            if column in ["InitializationTime","ExecutionTime","WaitInInitialization","WaitInExecution","PerfLeader","PerfMember"]:
                protocol_grouped_df[column] = protocol_grouped_df[column] / 64
        grouped_dfs[protocol] = protocol_grouped_df
        dfs[protocol] = protocol_df

    if not os.path.exists("./plots"):
        os.mkdir("./plots")  # create plot directory inside res
    os.chdir("./plots")
    
    plot_params = ["TotalTime", "InitializationTime", "ExecutionTime","WaitInInitialization","WaitInExecution","PerfLeader","PerfMember"]
    my_plot = plot.Plot(
        VARYING_TYPE,
        x_label,
        protocols,
        plot_params,  # change
    )  # change

    my_plot.plot_all_param_all_protocol(grouped_dfs)
    my_plot.plot_all_param_per_core("caracal", dfs["caracal"])
    my_plot.plot_all_param_per_core("serval", dfs["serval"])
    my_plot.plot_cache_hit_rate(grouped_dfs)

    os.chdir("../../../../")  # go back to base directory


if __name__ == "__main__":
    build()
    run_all()
    plot_all()