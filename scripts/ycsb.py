#!/usr/bin/env python3

import os
from sys import executable

import pandas as pd

import datetime


import module.plot as plot
import module.setting as setting

# EXECUTE THIS SCRIPT IN BASE DIRECTORY!!!

CLOCKS_PER_US = 2100  # used in plot
NUM_EXPERIMENTS_PER_SETUP = 1  # used in plot
NUM_SECONDS = 1  # used in plot
VARYING_TYPE = "contention"  # used in plot

x_label = {
    "num_threads": "#thread",
    "reps": "#operations",
    "contention": "Skew",
}

protocol_id = {"CARACAL": 0, "SERVAL": 1}

protocols = ["caracal", "serval"]

def gen_setups():
    payloads = [4]
    workloads = ["X"] # Write Only
    # workloads = ["A"]  # 50:50
    # workloads = ["Y"] # 60:40
    records = [10000]
    threads = [64]
    skews = [0.5, 0.99] # 0.0 - 0.99
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
            "cmake .. -DLOG_LEVEL=0 -DCMAKE_BUILD_TYPE=Debug -DBENCHMARK=YCSB -DCC_ALG="
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
        os.mkdir("./tmp")
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
                # "numactl --interleave=all ./" # 多分これするとregionが壊れる
                "./"
                + title
                + " "
                + " ".join([str(NUM_SECONDS), *args, str(exp_id)])
                + " > ./tmp/"
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

    dfs = {}
    grouped_dfs = {}
    for protocol in protocols:
        tmp = df[df["protocol"] == protocol]
        grouped_tmp = tmp.groupby(runtime_param, as_index=False).sum()
        grouped_dfs[protocol] = grouped_tmp
        dfs[protocol] = tmp

    if not os.path.exists("./plots"):
        os.mkdir("./plots")  # create plot directory inside res
    os.chdir("./plots")
    
    plot_params = ["TotalTime", "InitializationTime", "ExecutionTime"]
    my_plot = plot.Plot(
        VARYING_TYPE,
        x_label,
        CLOCKS_PER_US,
        protocols,
        plot_params,  # change
    )  # change

    my_plot.plot_all_param_all_protocol(grouped_dfs)
    my_plot.plot_all_param_per_core("caracal", dfs["caracal"])
    my_plot.plot_all_param_per_core("serval", dfs["serval"])
    # my_plot.plot_all_param(dfs["serval"])

    os.chdir("../../../../")  # go back to base directory


if __name__ == "__main__":
    build()
    run_all()
    plot_all()
