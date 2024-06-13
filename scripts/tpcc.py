#!/usr/bin/env python3

import os
from sys import executable

import pandas as pd

import datetime

import module.plot as plot
import module.setting as setting

# EXECUTE THIS SCRIPT IN BASE DIRECTORY!!!

CLOCKS_PER_US = 2100
NUM_EXPERIMENTS_PER_SETUP = 1
NUM_SECONDS = 100
VARYING_TYPE = "num_warehouses"

x_label = {
    "num_threads": "#thread",
    "sleep": "sleep[μs] (Long)",
    "num_warehouses": "#num_warehouse",
    "opt_interval": "opt_interval",
}

def gen_setups():
    gc_modes = [2, 1]
    # gc_modes = [3]

    # num_warehouses = [1]
    # num_warehouses = [1]
    num_warehouses = [54]
    #num_threads = [1, 14, 24, 34, 44, 54, 64]
    # num_threads = [1]
    #num_threads = [1, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64]
    num_threads = [64]
    exist_longs = [0]
    sleeps = [0]
    intervals = [0]
    opt_intervals = [20]

    return [
        [
            [str(gc_mode)],
            [
                str(num_warehouse), # TODO: change
                str(num_thread),
                str(exist_long),
                str(sleep),
                str(interval),
                str(opt_interval),
            ],
        ]
        for gc_mode in gc_modes
        for num_warehouse in num_warehouses
        for num_thread in num_threads
        for exist_long in exist_longs
        for sleep in sleeps
        for interval in intervals
        for opt_interval in opt_intervals
    ]


def build():
    if not os.path.exists("./build"):
        os.mkdir("./build")  # create build
    os.chdir("./build")
    if not os.path.exists("./log"):
        os.mkdir("./log")  # compile logs
    for setup in gen_setups():
        [[gc_mode], _] = setup
        print("Compiling GC_MODE=" + gc_mode)
        logfile = "GC_MODE_" + gc_mode + ".compile_log"
        os.system(
            "cmake .. -DLOG_LEVEL=0 -DCMAKE_BUILD_TYPE=Debug -DBENCHMARK=TPCC -DCC_ALG=SI -DGC_MODE="
            + gc_mode
            + " > ./log/"
            + "compile_" + logfile + " 2>&1"
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
            [gc_mode],
            args,
        ] = setup

        title = "tpcc" + "_si_" + gc_mode

        print("[{}: {}]".format(title, " ".join([str(NUM_SECONDS), *args])))

        for exp_id in range(NUM_EXPERIMENTS_PER_SETUP):
            dt_now = datetime.datetime.now()
            print(" Trial:" + str(exp_id))
            print(
                "./"
                + title
                + " "
                + " ".join([str(NUM_SECONDS), *args, str(exp_id)])
                + " > ./tmp/"
                + gc_mode
                + "_"
                + str(dt_now.isoformat())
                + " 2>&1"
            )
            ret = os.system(
                "numactl --interleave=all ./"
                + title
                + " "
                + " ".join([str(NUM_SECONDS), *args, str(exp_id)])
                + " > ./tmp/"
                + gc_mode
                + "_"
                + str(dt_now.isoformat())
                + " 2>&1"
            )
            if ret != 0:
                print("Error. Stopping")
                exit(0)
    ret = os.system("cat ./res/*.csv > ./res/result.csv; cat ./res/header > ./res/concat.csv; cat ./res/result.csv >> ./res/concat.csv")
    if ret != 0:
        print("Error. Stopping")
        exit(0)
    os.chdir("../../")  # back to base directory


def plot_all():
    path = "build/bin/res"
    # plot throughput
    os.chdir(path)  # move to result file
    exp_param = pd.read_csv("header", sep=',').columns.tolist()
    tpcc_param = pd.read_csv("tpcc_param", sep=',').columns.tolist()
    countable_param = [i for i in exp_param if i not in tpcc_param]
    tpcc_param.remove('exp_id')
    df = pd.read_csv("result.csv", sep=',', names=exp_param)
        
    if not os.path.exists("./plots"):
        os.mkdir("./plots")  # create plot directory inside res
    os.chdir("./plots")

    dfs = {}
    for i in range(len(setting.gc_names)):
        tmp = df[df["GC_MODE"] == i]
        if not tmp.empty:
            tmp = tmp.groupby(tpcc_param, as_index=False).sum()
            for type in countable_param:
                tmp[type] = tmp[type] / NUM_SECONDS / NUM_EXPERIMENTS_PER_SETUP
            # TODO: MIN MAX LATENCYに対応してない。NUM_SECONDSの割り算を削除する必要がある。
        dfs[setting.gc_names[i]] = tmp.sort_values('num_warehouses', ascending=False)
        # dfs[setting.gc_names[i]] = tmp

    my_plot = plot.Plot(VARYING_TYPE, x_label, CLOCKS_PER_US, ["epo", "epo-r"], exp_param)  # change
    my_plot.plot_tps_ratio(dfs, "epo-r", "epo", ["total"]) # change
    my_plot.plot_tps(dfs, ["total"])
    my_plot.plot_memory(dfs)
    my_plot.plot_urv(dfs["epo"])
    my_plot.plot_read_and_write(dfs)
    my_plot.plot_traversal_tpcc(dfs)
    my_plot.plot_avg_latency_tpcc(dfs)
    # my_plot.plot_avg_latency(dfs, ["short_total_latency", "long_total_latency"])
    # plot_latency(dfs)
    my_plot.do_plot_tpcc(dfs)
  
    os.chdir("../../../../")  # go back to base directory


if __name__ == "__main__":
    build()
    run_all()
    plot_all()
