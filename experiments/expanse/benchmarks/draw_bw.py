import pandas as pd
import os,sys
from matplotlib import pyplot as plt
sys.path.append("../../include")
from draw_simple import *

name = "bw"
input_path = "data/"
all_labels = ["task", "Size(B)", "latency(us)", "bandwidth(MB/s)"]

def interactive(df):
    tasks = [
             'lci 2m',
             'lci 2m dyn',
             ]

    df1 = df[df.apply(lambda row:
                         row["task"] in tasks and
                      True,
                      axis=1)]
    x_key = "Size(B)"
    y_key = "bandwidth(MB/s)"
    tag_key = "task"
    lines = parse_tag(df1, x_key, y_key, tag_key)
    for line in lines:
        # if line["label"] == "ibv_pingpong_read -t 0":
        #     line["y"] = list(map(lambda y: y*2, line["y"]))
        print(line)
        plt.errorbar(line["x"], line["y"], line["error"], label=line['label'], marker='.', markerfacecolor='white', capsize=3)
    plt.xlabel(x_key)
    plt.ylabel(y_key)
    plt.legend()
    plt.show()

def plot(df, x_key, y_key, tag_key, title):
    fig, ax = plt.subplots()
    lines = parse_tag(df, x_key, y_key, tag_key)
    for line in lines:
        ax.errorbar(line["x"], line["y"], line["error"], label=line['label'], marker='.', markerfacecolor='white', capsize=3)
    ax.set_xlabel(x_key)
    ax.set_ylabel(y_key)
    ax.set_title(title)
    ax.legend()
    output_png_name = os.path.join("draw", "{}.png".format(title))
    fig.savefig(output_png_name)

def batch(df):
    plot(df, "Size(B)", "bandwidth(MB/s)", "task", "bw")

if __name__ == "__main__":
    df = pd.read_csv(os.path.join(input_path, name + ".csv"))
    interactive(df)
    # batch(df)
