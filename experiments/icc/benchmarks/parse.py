import re
import glob
import numpy as np
import ast
import pandas as pd
import os,sys
sys.path.append("../../include")
from draw_simple import *

name = "all"
input_path = "run/slurm_output.*-o*"
output_path = "data/"
edge_srun = {
    "format": "srun (\S+) (.+)",
    "label": ["job", "task"],
}
edge_data = {
    "format": "(\d+)\s+(\S+)\s+(\S+)\s+(\S+).+",
    "label": ["Size(B)", "latency(us)", "message rate(mps)", "bandwidth(MB/s)"]
}
all_labels = edge_srun["label"] + edge_data["label"]

def get_typed_value(value):
    if value == '-nan':
        return np.nan
    try:
        typed_value = ast.literal_eval(value)
    except:
        typed_value = value
    return typed_value

if __name__ == "__main__":
    filenames = glob.glob(input_path)
    lines = []
    for filename in filenames:
        with open(filename) as f:
            lines.extend(f.readlines())

    df = pd.DataFrame(columns=all_labels)
    state = "init"
    current_entry = dict()
    for line in lines:
        line = line.strip()
        m = re.match(edge_srun["format"], line)
        if m:
            current_entry = dict()
            current_data = [get_typed_value(x) for x in m.groups()]
            current_label = edge_srun["label"]
            for label, data in zip(current_label, current_data):
                current_entry[label] = data
            continue

        m = re.match(edge_data["format"], line)
        if m:
            current_data = [get_typed_value(x) for x in m.groups()]
            current_label = edge_data["label"]
            for label, data in zip(current_label, current_data):
                current_entry[label] = data
            df = pd.concat([df, pd.DataFrame([current_entry])], ignore_index=True, sort=True)
            continue
    if df.shape[0] == 0:
        print("Error! Get 0 entries!")
        exit(1)
    else:
        print("get {} entries".format(df.shape[0]))
    df["Size(B)"] = np.log2(df["Size(B)"].astype(np.float32)).astype(int)
    df["latency(us)"] = df["latency(us)"] / 2
    df.to_csv(os.path.join(output_path, "{}.csv".format(name)))