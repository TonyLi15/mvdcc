import matplotlib.pyplot as plt
import matplotlib.ticker as ptick  ##これが必要！


class Plot:
    markersize = 20

    def __init__(self, VARYING_TYPE, x_label, CLOCKS_PER_US, protocols, plot_params):
        self.VARYING_TYPE = VARYING_TYPE
        self.x_label = x_label
        self.clocks = {
            "CLOCKS_PER_NS": (CLOCKS_PER_US / 1000),
            "CLOCKS_PER_US": CLOCKS_PER_US,
            "CLOCKS_PER_MS": (CLOCKS_PER_US * 1000),
            "CLOCKS_PER_S": (CLOCKS_PER_US * 1000 * 1000),
        }
        self.clocks_label = {
            "CLOCKS_PER_NS": "[ns]",
            "CLOCKS_PER_US": "[μs]",
            "CLOCKS_PER_MS": "[ms]",
            "CLOCKS_PER_S": "[s]",
        }
        self.protocols = protocols
        self.plot_params = plot_params


    def get_x_ticks(self, df, column):
        return [str(req) for req in df[column]]
    
    node_color = {"node0": "red", "node1": "blue"}
    def plot_all_param_per_core(self, protocol_name, df):
        for param in self.plot_params:
            fig, ax1 = plt.subplots(dpi=300)
            for core in df['Core'].unique():
                per_core_df = df[df["Core"] == core] # coreθの結果のみ取り出す
                node_key = "node" + str(per_core_df["Node"].values[0]) # coreθのNode番号を取得
                node_color = self.node_color[node_key]
                ax1.plot(
                    self.get_x_ticks(per_core_df, self.VARYING_TYPE),
                    per_core_df[param],
                    markersize=self.markersize,
                    clip_on=False,
                    color=node_color
            )
            fig.tight_layout()
            plt.tick_params(labelsize=19)
            plt.xticks(rotation=40)
            plt.xlabel(self.x_label[self.VARYING_TYPE], fontsize=25)
            plt.grid()
            plt.savefig(
                protocol_name + "_per core_" + param + "_varying_" + self.VARYING_TYPE + ".pdf",
                bbox_inches="tight",
            )
            plt.savefig(
                protocol_name + "_per core_" + param + "_varying_" + self.VARYING_TYPE + ".eps",
                bbox_inches="tight",
            )

    def plot_all_param_all_protocol(self, dfs):
        for param in self.plot_params:
            fig, ax1 = plt.subplots(dpi=300)
            for protocol in self.protocols:
                df = dfs[protocol]
                ax1.plot(
                    self.get_x_ticks(df, self.VARYING_TYPE),
                    df[param],
                    markersize=self.markersize,
                    clip_on=False,
                    label=protocol
                )
            fig.tight_layout()
            plt.tick_params(labelsize=19)
            plt.legend(fontsize=19)
            plt.xticks(rotation=40)
            plt.xlabel(self.x_label[self.VARYING_TYPE], fontsize=25)
            plt.grid()
            plt.savefig(
                param + "_varying_" + self.VARYING_TYPE + ".pdf",
                bbox_inches="tight",
            )
            plt.savefig(
                param + "_varying_" + self.VARYING_TYPE + ".eps",
                bbox_inches="tight",
            )

