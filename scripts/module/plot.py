import matplotlib.pyplot as plt
import matplotlib.ticker as ptick  ##これが必要！

import numpy as np # For np.arange


class Plot:
    markersize = 15
    marker = {"caracal": "x", "serval": "^", "serval_rc": "+", "serval_rc_bbu": "o", "cheetah": "8"}
    # color = {"caracal": "red", "serval": "blue", "serval_rc": "green", "serval_rc_bbu": "orange"}
    # color = {"caracal": "#DB6A2F", "serval": "#4EAC7F", "serval_rc": "#2F6DB6", "serval_rc_bbu": "#2F6DB6"}
    color = {"caracal": "#266d8e", "serval": "#023043", "serval_rc": "#f26d3d", "serval_rc_bbu": "#f26d3d", "cheetah": "#ff9ad0"}
    protocol_names = {"caracal": "Caracal", "serval": "Serval", "serval_rc": "Cheetah--", "serval_rc_bbu": "Cheetah-", "cheetah": "Proposed Method"}
    linestyle = {"caracal": "solid", "serval": "dashed", "serval_rc": "dashdot", "serval_rc_bbu": "dashdot", "cheetah": "dotted"}
    ylabel = {
      "Core": "Core",
      "Node": "Node",
      "Create": "Version Created Per-core",
      "Delete": "Reclaimed Versions",
      "TotalTime": "Total Latency",
      "InitializationTime": "Initialization Latency",
      "ExecutionTime": "Execution Latency",
      "Sync1Time": "Sync1 Latency",
      "Sync2Time": "Sync2 Latency",
      "WaitInInitialization": "Wait in Initialization",
      "WaitInExecution": "Wait in Execution",
      "WaitInGC": "Wait in GC",
      "AppendTime": "AppendTime",
      "ExecReadTime": "ExecReadTime",
      "ExecWriteTime": "ExecWriteTime",
      "PerfLeader": "Perf Leader", # change
      "PerfMember": "Perf Member", # change,
  }

    def __init__(self, VARYING_TYPE, x_label, protocols, plot_params):
        self.VARYING_TYPE = VARYING_TYPE
        self.x_label = x_label
        # self.clocks = {
        #     "CLOCKS_PER_NS": (CLOCKS_PER_US / 1000),
        #     "CLOCKS_PER_US": CLOCKS_PER_US,
        #     "CLOCKS_PER_MS": (CLOCKS_PER_US * 1000),
        #     "CLOCKS_PER_S": (CLOCKS_PER_US * 1000 * 1000),
        # }
        # self.clocks_label = {
        #     "CLOCKS_PER_NS": "[ns]",
        #     "CLOCKS_PER_US": "[μs]",
        #     "CLOCKS_PER_MS": "[ms]",
        #     "CLOCKS_PER_S": "[s]",
        # }
        self.protocols = protocols
        self.plot_params = plot_params


    def get_x_ticks(self, df, column):
        return [str(req) for req in df[column]]
    
    def organize_data_by_param(self, df, param):
        CLOCKS_PER_US = 2100 # TODO
        CLOCKS_PER_MS = CLOCKS_PER_US * 1000
        CLOCKS_PER_SEC = CLOCKS_PER_MS * 1000
        if "Time" in param:
            return df[param] / CLOCKS_PER_SEC
        elif "Wait" in param:
            return df[param] / CLOCKS_PER_MS
        return df[param]

    def organize_ylabel_by_param(self, param):
        if "Time" in param:
            return self.ylabel[param] + " [s]"
        elif "Wait" in param:
            return self.ylabel[param] + " [ms]"
        return self.ylabel[param]

    # node_color = {"node0": "red", "node1": "blue"}
    # def plot_all_param_per_core(self, protocol_name, df):
    #     for param in self.plot_params:
    #         fig, ax1 = plt.subplots(dpi=300)
    #         node_labels = []
    #         node_lines = []
    #         for core in df['Core'].unique():
    #             per_core_df = df[df["Core"] == core] # coreθの結果のみ取り出す
    #             node_label = "node" + str(per_core_df["Node"].values[0]) # coreθのNode番号を取得
    #             node_color = self.node_color[node_label]
    #             line, = ax1.plot(
    #                 self.get_x_ticks(per_core_df, self.VARYING_TYPE),
    #                 self.organize_data_by_param(per_core_df, param),
    #                 markersize=self.markersize,
    #                 clip_on=False,
    #                 color=node_color,
    #                 label=node_label
    #             )
    #             if node_label not in node_labels:
    #                 node_lines.append(line)
    #                 node_labels.append(node_label)
    #         fig.tight_layout()
    #         plt.rcParams['text.usetex'] = False
    #         plt.rcParams['font.family'] = 'serif'
    #         plt.legend(node_lines, node_labels, fontsize=19)
    #         plt.tick_params(labelsize=19)
    #         plt.xticks(rotation=40)
    #         plt.xlabel(self.x_label[self.VARYING_TYPE], fontsize=25)
    #         plt.ylabel(self.organize_ylabel_by_param(param) + " " + protocol_name, fontsize=15)
    #         plt.grid()
    #         plt.savefig(
    #             protocol_name + "_per core_" + param + "_varying_" + self.VARYING_TYPE + ".pdf",
    #             bbox_inches="tight",
    #         )
    #         # plt.savefig(
    #         #     protocol_name + "_per core_" + param + "_varying_" + self.VARYING_TYPE + ".eps",
    #         #     bbox_inches="tight",
    #         # )

    # def plot_cache_hit_rate(self, dfs):
    #     fig, ax1 = plt.subplots(dpi=300)
    #     for protocol in self.protocols:
    #         df = dfs[protocol]
    #         ax1.plot(
    #             self.get_x_ticks(df, self.VARYING_TYPE),
    #             df["PerfMember"]/df["PerfLeader"]*100, # miss / all reference
    #             markersize=self.markersize,
    #             clip_on=False,
    #             label=protocol,
    #             marker=self.marker[protocol],
    #             color=self.color[protocol]
    #         )
    #     fig.tight_layout()
    #     plt.rcParams['text.usetex'] = False
    #     plt.rcParams['font.family'] = 'serif'
    #     plt.tick_params(labelsize=19)
    #     plt.legend(fontsize=19)
    #     plt.xticks(rotation=40)
    #     plt.xlabel(self.x_label[self.VARYING_TYPE], fontsize=25)
    #     plt.ylabel("Cache Miss Rate", fontsize=25)
    #     plt.grid()
    #     plt.savefig(
    #         "Cache Miss Rate" + "_varying_" + self.VARYING_TYPE + ".pdf",
    #         bbox_inches="tight",
    #     )
    #     # plt.savefig(
    #     #     "Cache Miss Rate" + "_varying_" + self.VARYING_TYPE + ".eps",
    #     #     bbox_inches="tight",
    #     # )

    def plot_all_param_all_protocol(self, dfs):
        for param in self.plot_params:
            plt.rcParams['text.usetex'] = False
            plt.rcParams['font.family'] = 'serif'
            fig, ax1 = plt.subplots(dpi=300)
            for protocol in self.protocols:
                df = dfs[protocol]
                ax1.plot(
                    self.get_x_ticks(df, self.VARYING_TYPE),
                    self.organize_data_by_param(df, param),
                    markersize=self.markersize,
                    clip_on=False,
                    linestyle=self.linestyle[protocol],
                    label=self.protocol_names[protocol],
                    marker=self.marker[protocol],
                    color=self.color[protocol],
                    linewidth=3,
                    fillstyle='none',
                    markeredgewidth=3
                    # markeredgecolor=self.color[protocol],  
                )
            fig.tight_layout()
            plt.tick_params(labelsize=19)
            plt.legend(fontsize=19)
            plt.xticks(rotation=40)

            # ax = plt.gca() # get current axes 現在の軸設定データを取得する
            # y_min, y_max = ax.get_ylim()
            # plt.ylim(0, y_max)

            plt.xlabel(self.x_label[self.VARYING_TYPE], fontsize=25)
            plt.ylabel(self.organize_ylabel_by_param(param), fontsize=25)
            plt.grid()
            plt.savefig(
                param + "_varying_" + self.VARYING_TYPE + ".pdf",
                bbox_inches="tight",
            )
           


    def histogram_of_init_and_exec_phase(self, dfs, threshold):
        protocol_labels = []
        results = {"InitializationTime": [], "ExecutionTime": []}
        for protocol in self.protocols:
            protocol_labels.append(self.protocol_names[protocol])
            df = dfs[protocol]
            for param in ["InitializationTime","ExecutionTime"]:
              results[param].append(self.organize_data_by_param(df[df[self.VARYING_TYPE] == threshold], param).values[0])
      
        plt.rcParams['text.usetex'] = False
        plt.rcParams['font.family'] = 'serif'
        fig, ax1 = plt.subplots(dpi=300)

        # changed x so that it fits the number of protocols in experiments
        x = np.arange(1, len(self.protocols) + 1)
        g1 = plt.bar(x, results["InitializationTime"], align="edge", width=-0.3, hatch='xx',fill=None,label="Initialization")
        g2 = plt.bar(x, results["ExecutionTime"], align="edge", width= 0.3, hatch='/',fill=None,label="Execution")
        
        plt.legend(handles=[g1,g2], fontsize=19)
        plt.tick_params(labelsize=19)
        plt.xticks(x, protocol_labels, fontsize=25)
        plt.ylabel('Time [S]', fontsize=25)
        plt.grid()
        plt.savefig(
            "histogram_of_init_and_exec_phase" + ".pdf",
            bbox_inches="tight",
        )

    # def plot_all_param(self, protocol, df):
    #     for param in self.plot_params:
    #         plt.rcParams['text.usetex'] = False
    #         plt.rcParams['font.family'] = 'serif'
    #         fig, ax1 = plt.subplots(dpi=300)
    #         ax1.plot(
    #             self.get_x_ticks(df, self.VARYING_TYPE),
    #             self.organize_data_by_param(df, param),
    #             markersize=self.markersize,
    #             clip_on=False,
    #             label=protocol,
    #             marker=self.marker[protocol],
    #             color=self.color[protocol]
    #         )
    #         fig.tight_layout()
    #         plt.tick_params(labelsize=19)
    #         plt.legend(fontsize=19)
    #         plt.xticks(rotation=40)
    #         plt.xlabel(self.x_label[self.VARYING_TYPE], fontsize=25)
    #         plt.ylabel(self.organize_ylabel_by_param(param), fontsize=25)
    #         plt.grid()
    #         plt.savefig(
    #             protocol + "_" + param + "_varying_" + self.VARYING_TYPE + ".pdf",
    #             bbox_inches="tight",
    #         )
    #         # plt.savefig(
    #         #     protocol + "_" + param + "_varying_" + self.VARYING_TYPE + ".eps",
    #         #     bbox_inches="tight",
    #         # )