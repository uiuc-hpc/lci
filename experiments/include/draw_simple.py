#!/usr/bin/env python3

import numpy as np
import matplotlib
# matplotlib.use('Agg')
from matplotlib import pyplot as plt
from matplotlib.ticker import ScalarFormatter, FormatStrFormatter
import pandas as pd
import sys,os
import json

def plot(title, xlabel, ylabel, data, fname='out.pdf', is_show=False, is_save=False, xscale="log", yscale="log"):
    fig, ax = plt.subplots()

    domain = set()
    lrange = set()

    for datum in data:
        for x in datum['x']:
            domain.add(x)
        for y in datum['y']:
            lrange.add(y)

    domain = sorted(domain)

    for datum in data:
        marker='.'
        linestyle=None
        color=None
        if 'marker' in datum:
            marker = datum['marker']
        if 'linestyle' in datum:
            # print('Setting linestyle for %s to %s' % (datum['label'], datum['linestyle']))
            linestyle = datum['linestyle']
        if 'color' in datum:
            color = datum['color']
        if 'error' in datum:
            ax.errorbar(datum['x'], datum['y'], datum['error'], label=datum['label'], marker=marker, linestyle=linestyle, color=color, markerfacecolor='white', capsize=3)
        else:
            ax.plot(datum['x'], datum['y'], label=datum['label'], marker=marker, linestyle=linestyle, color=color, markerfacecolor='white')

    ymin = min(lrange)
    ymax = max(lrange)

    ytick_range = list([2**x for x in range(-5, 40)])
    yticks = []
    for tick in ytick_range:
        if tick >= ymin and tick <= ymax:
            yticks.append(tick)
    if len(yticks) > 0: yticks.append(yticks[-1]*2)
    if len(yticks) > 0: yticks.append(yticks[0]/2)

    ax.minorticks_off()
    ax.set_yscale("log")
    ax.set_xscale("log")
    ax.set_xticks(domain)
    ax.set_xticklabels(list(map(lambda x: str(x), domain)))
    if len(yticks) > 0: ax.set_yticks(yticks)
    ax.yaxis.set_major_formatter(FormatStrFormatter('%.2f'))
    if (len(yticks) < 5):
        ax.yaxis.set_minor_formatter(FormatStrFormatter('%.2f'))
    else:
        ax.yaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax.xaxis.set_major_formatter(matplotlib.ticker.ScalarFormatter())
    ax.xaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())

    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.legend(loc='best')
    plt.tight_layout()
    if is_show:
        plt.show()
    if is_save:
        plt.savefig(fname)

def line_plot(title, xlabel, ylabel, data, fname='out.pdf', is_show=False, is_save=False, xscale="log", yscale="log"):
    fig, ax = plt.subplots()

    domain = set()
    lrange = set()

    for datum in data:
        for x in datum['x']:
            domain.add(x)
        for y in datum['y']:
            lrange.add(y)

    domain = sorted(domain)

    for datum in data:
        marker='.'
        linestyle=None
        color=None
        if 'marker' in datum:
            marker = datum['marker']
        if 'linestyle' in datum:
            # print('Setting linestyle for %s to %s' % (datum['label'], datum['linestyle']))
            linestyle = datum['linestyle']
        if 'color' in datum:
            color = datum['color']
        if 'error' in datum:
            ax.errorbar(datum['x'], datum['y'], datum['error'], label=datum['label'], marker=marker, linestyle=linestyle, color=color, markerfacecolor='white', capsize=3)
        else:
            ax.plot(datum['x'], datum['y'], label=datum['label'], marker=marker, linestyle=linestyle, color=color, markerfacecolor='white')

    ymin = min(lrange)
    ymax = max(lrange)

    ytick_range = list([2**x for x in range(-5, 40)])
    yticks = []
    for tick in ytick_range:
        if tick >= ymin and tick <= ymax:
            yticks.append(tick)
    if len(yticks) > 0: yticks.append(yticks[-1]*2)
    if len(yticks) > 0: yticks.append(yticks[0]/2)

    ax.minorticks_off()
    ax.set_yscale("log")
    ax.set_xscale("log")
    ax.set_xticks(domain)
    ax.set_xticklabels(list(map(lambda x: str(x), domain)))
    if len(yticks) > 0: ax.set_yticks(yticks)
    ax.yaxis.set_major_formatter(FormatStrFormatter('%.2f'))
    if (len(yticks) < 5):
        ax.yaxis.set_minor_formatter(FormatStrFormatter('%.2f'))
    else:
        ax.yaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax.xaxis.set_major_formatter(matplotlib.ticker.ScalarFormatter())
    ax.xaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())

    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.legend(loc='best')
    plt.tight_layout()
    if is_show:
        plt.show()
    if is_save:
        plt.savefig(fname)

def draw_simple(config):
    df = pd.read_csv(config["input"])
    lines = []
    x_key = config["x_key"]
    y_key = config["y_key"]
    title = config["name"]

    current_domain = []
    current_value = []
    current_error = []
    for x in df[x_key].unique():
        y = df[df[x_key] == x].median()[y_key]
        error = df[df[x_key] == x].std()[y_key]
        if y is np.nan:
            continue
        if y == 0:
            continue
        current_domain.append(float(x))
        current_value.append(float(y))
        current_error.append(float(error))
    lines.append({'label': "", 'x': current_domain, 'y': current_value, 'error': current_error})

    with open(os.path.join(config["output"], "{}.json".format(config["name"])), 'w') as outfile:
        json.dump(lines, outfile)
    line_plot(title, x_key, y_key, lines, os.path.join(config["output"], "{}.png".format(config["name"])), False, is_show=False, is_save=True)

def parse_tag(df, x_key, y_key, tag_key):
    lines = []

    for tag in df[tag_key].unique():
        criterion = (df[tag_key] == tag)
        df1 = df[criterion]
        current_domain = []
        current_value = []
        current_error = []
        for x in df1[x_key].unique():
            y = df1[df1[x_key] == x].median()[y_key]
            error = df1[df1[x_key] == x].std()[y_key]
            if y is np.nan:
                continue
            if y == 0:
                continue
            current_domain.append(float(x))
            current_value.append(float(y))
            current_error.append(float(error))
        lines.append({'label': str(tag), 'x': current_domain, 'y': current_value, 'error': current_error})
    return lines

def draw_tag(config, df = None, drawError = True, isShow = False, isSave = True):
    if df is None:
        df = pd.read_csv(config["input"])
    lines = []
    x_key = config["x_key"]
    y_key = config["y_key"]
    tag_key = config["tag_key"]
    title = "{}".format(config["name"])

    lines = parse_tag(df, x_key, y_key, tag_key)

    if len(lines) == 0:
        print("Error! Got 0 line!")
        exit(1)

    output_png_name = "{}.png".format(config["name"])
    if "output" in config:
        with open(os.path.join(config["output"], "{}.json".format(config["name"])), 'w') as outfile:
            json.dump(lines, outfile)
        output_png_name = os.path.join(config["output"], "{}.png".format(config["name"]))
    line_plot(title, x_key, y_key, lines, fname=output_png_name, is_show=isShow, is_save=isSave)

def draw_tags(config, df = None, drawError = True, isShow = False, isSave = True):
    if df is None:
        df = pd.read_csv(config["input"])
    lines = []
    x_key = config["x_key"]
    y_key = config["y_key"]
    tag_keys = config["tag_keys"]
    title = "{}".format(config["name"])

    for index in df[tag_keys].drop_duplicates().index:
        criterion = True
        tags = []
        for tag_key in tag_keys:
            criterion = (df[tag_key] == df[tag_key][index]) & criterion
            tags.append(df[tag_key][index])
        df1 = df[criterion]
        current_domain = []
        current_value = []
        current_error = []
        for x in df1[x_key].unique():
            y = df1[df1[x_key] == x].median()[y_key]
            error = df1[df1[x_key] == x].std()[y_key]
            if y is np.nan:
                continue
            if y == 0:
                continue
            current_domain.append(float(x))
            current_value.append(float(y))
            current_error.append(float(error))
        if "label" in config:
            label = config["label"].format(*tags)
        else:
            label = str(tags)
        if drawError:
            lines.append({'label': label, 'x': current_domain, 'y': current_value, 'error': current_error})
        else:
            lines.append({'label': label, 'x': current_domain, 'y': current_value})

    if len(lines) == 0:
        print("Error! Got 0 line!")
        exit(1)

    output_png_name = "{}.png".format(config["name"])
    if "output" in config:
        with open(os.path.join(config["output"], "{}.json".format(config["name"])), 'w') as outfile:
            json.dump(lines, outfile)
        output_png_name = os.path.join(config["output"], "{}.png".format(config["name"]))
    line_plot(title, x_key, y_key, lines, fname=output_png_name, is_show=isShow, is_save=isSave)