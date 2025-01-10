#!/usr/bin/env python3

from parser_graph import ParserNode, ParserGraph, file_to_lines
import sys,os
import json

def parse_simple(config):
    node0 = ParserNode(
        config["format"],
        config["label"],
        name="data"
    )
    all_labels = config["label"]
    lines = file_to_lines(config["input"])

    graph = ParserGraph(node0, all_labels)
    df = graph.parse(lines)
    df.sort_values(by=all_labels, inplace=True)
    print("get {} entries".format(df.shape[0]))
    df.to_csv(os.path.join(config["output"], "{}.csv".format(config["name"])))
    return df

def parse_srun(config):
    node0 = ParserNode(
        config["srun_format"],
        config["srun_label"],
        name="srun"
    )
    node1 = ParserNode(
        config["data_format"],
        config["data_label"],
        name="data"
    )
    node0.connect([node1])

    all_labels = config["srun_label"] + config["data_label"]
    lines = file_to_lines(config["input"])

    graph = ParserGraph(node0, all_labels)
    df = graph.parse(lines)
    df.sort_values(by=all_labels, inplace=True)
    print("get {} entries".format(df.shape[0]))
    df.to_csv(os.path.join(config["output"], "{}.csv".format(config["name"])))

if __name__ == "__main__":
    import argparse
    import pathlib

    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", help="input configure file path", type=pathlib.Path)
    args = parser.parse_args()
    if args.config:
        with open(args.config, 'r') as infile:
            config = json.load(infile)
        parse_simple(config)
    else:
        parser.print_help()