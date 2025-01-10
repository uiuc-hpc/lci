import pandas as pd
import os,sys
from matplotlib import pyplot as plt
sys.path.append("../../include")
from draw_simple import *

name = "all"
input_path = "data/"
all_labels = ["job", "type", "Size(B)", "latency(us)", "message rate(mps)", "bandwidth(MB/s)"]

def interactive(df):
    tasks = {
        "MPI" : "MPI one core",
        "MPI everywhere 64": "MPI multi-process",
        "MPI multithread 64": "MPI multi-thread",
        # "LCI 2m multithread 64 dyn": "LCI multi-thread",
    }
    df1 = df[df.apply(lambda row:
                      (row["job"] == "mlt") and
                      (row["task"] in tasks) and
                      (row["Size(B)"] <= 16) and
                      True,
                      axis=1)]
    x_key = "Size(B)"
    y_key = "message rate(mps)"
    tag_key = "task"
    lines = parse_tag(df1, x_key, y_key, tag_key)
    for line in lines:
        # if line["label"] == "ibv_pingpong_read -t 0":
        #     line["y"] = list(map(lambda y: y*2, line["y"]))
        print(line)
        plt.errorbar(line["x"], line["y"], line["error"], label=tasks[line['label']], marker='.', markerfacecolor='white', capsize=3)
    plt.xlabel("Size(2^# B)")
    plt.ylabel(y_key)
    plt.legend()
    plt.title("Multithreaded Ping-pong Benchmark on SDSC Expanse with 64 cores")
    plt.show()


def plot(df, x_key, y_key, tag_key, title):
    fig, ax = plt.subplots()
    lines = parse_tag(df, x_key, y_key, tag_key)
    for line in lines:
        print(line)
        ax.errorbar(line["x"], line["y"], line["error"], label=line['label'], marker='.', markerfacecolor='white', capsize=3)
    ax.set_xlabel(x_key)
    ax.set_ylabel(y_key)
    ax.set_title(title)
    # ax.legend(bbox_to_anchor = (1.05, 0.6))
    ax.legend()
    output_png_name = os.path.join("draw", "{}.png".format(title))
    fig.savefig(output_png_name)

def batch(df):
    df1 = df[df.apply(lambda row:
                      row["job"] == "lt",
                      axis=1)]
    plot(df1, "Size(B)", "latency(us)", "task", "lt")

    df1 = df[df.apply(lambda row:
                      row["job"] == "bw",
                      axis=1)]
    plot(df1, "Size(B)", "bandwidth(MB/s)", "task", "bw")

if __name__ == "__main__":
    df = pd.read_csv(os.path.join(input_path, name + ".csv"))
    # interactive(df)
    batch(df)
