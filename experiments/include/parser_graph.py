import pandas as pd
import re
import glob
import ast
import numpy as np
from _collections import deque
from colorama import Fore, Back, Style

# _verbose = True
_verbose = False
_verboseprint = print if _verbose else lambda *a, **k: None

class ParserError(Exception):
    def __init__(self, str=""):
        _verboseprint("raise ParserError! " + str)

class ParserTerminate(Exception):
    def __init__(self, str=""):
        _verboseprint("raise ParserTerminate! " + str)

class ParserGraph:
    def __init__(self, node, all_labels):
        self.init_node = node
        self.labels = all_labels

    def parse(self, lines):
        lines = deque(lines)
        df = pd.DataFrame(columns=self.labels)
        current_node = self.init_node
        terminal_strings = [self.init_node.strings[0].strip()]
        data = []
        labels = []
        node_path = [(ParserNode(None,None,"fake"), [], [])]
        while lines:
            try:
                while True:
                    cdata, clabels = current_node.parse(lines, terminal_strings)
                    assert len(cdata) == len(clabels)
                    data.extend(cdata)
                    labels.extend(clabels)
                    node_path.append((current_node, data.copy(), labels.copy()))
                    _verboseprint("append node path: ", current_node.name, data, labels)

                    if not current_node.neighbors:
                        break
                    current_node = self.find_next(lines, current_node.neighbors, terminal_strings)
                    _verboseprint(Fore.BLUE + "Next node: ", current_node, "current data/label:", data, labels,
                                  Style.RESET_ALL)
                cdf = pd.DataFrame([data], columns=labels)
                _verboseprint(Fore.RED + "append df: ", cdf)
                _verboseprint(Style.RESET_ALL)
                df = df.append(cdf, ignore_index=True, sort=False)
                current_node = self.find_next(lines, list(zip(*node_path))[0], [])
                while node_path:
                    if node_path.pop()[0] == current_node:
                        break
                data = node_path[-1][1].copy()
                labels = node_path[-1][2].copy()
                _verboseprint(Fore.BLUE + "Next node: ", current_node, "current data/label:", data, labels, Style.RESET_ALL)
            except ParserTerminate:
                _verboseprint(Fore.RED, "Terminated reset ")
                _verboseprint(Style.RESET_ALL)
                current_node = self.init_node
                data = []
                labels = []
                node_path = [(ParserNode(None,None,"fake"), [], [])]
            except ParserError:
                break
        return df

    def find_next(self, lines, blocks, terminal_strings):
        while lines:
            line = lines[0].strip()
            for terminal_string in terminal_strings:
                if re.match(terminal_string, line):
                    raise ParserTerminate("find " + line)
            for block in blocks:
                if block.check(line):
                    return block
            lines.popleft()
        raise ParserError("ParserGraph::find_next: run out of lines")

class ParserNode:
    def __init__(self, strings, labels, name=""):
        if strings:
            self.strings = [string.strip() for string in strings.split("\n")]
        else:
            self.strings = None
        self.labels = labels
        self.neighbors = set()
        self.name = name

    def __str__(self):
        return hex(id(self)) + "-" +  self.name

    def __repr__(self):
        return hex(id(self)) + "-" + self.name

    def connect(self, neighbors):
        for item in neighbors:
            self.neighbors.add(item)

    def check(self, line):
        if not self.strings:
            return False
        format_string = self.strings[0]
        m = re.match(format_string, line)
        if m:
            return True
        else:
            return False

    def parse(self, lines, terminals = None):
        data = []
        format_strings = deque(self.strings)
        format_string = format_strings.popleft().strip()
        while lines:
            line = lines.popleft().strip()
            m = re.match(format_string, line)
            if m:
                data += [self.get_typed_value(x) for x in m.groups()]
                _verboseprint('Setting current tuple to %s based on "%s"' % (data,line))
                try:
                    format_string = format_strings.popleft().strip()
                except IndexError:
                    # _verboseprint("return: ", data, self.labels)
                    return data, self.labels
            else:
                for terminal_string in terminals:
                    if re.match(terminal_string, line):
                        raise ParserTerminate("find " + line)
                _verboseprint("Matching fails: {}///{}".format(line, format_string))

        raise ParserError("ParserBlock::parse: run out of lines!")

    def get_typed_value(self, value):
        if value == '-nan':
            return np.nan
        try:
            typed_value = ast.literal_eval(value)
        except:
            typed_value = value
        return typed_value

def file_to_lines(file_path):
    filenames = glob.glob(file_path)
    lines = []
    for filename in filenames:
        with open(filename) as f:
            lines.extend(f.readlines())
    return lines

if __name__ == "__main__":
    node0 = ParserNode(
        "srun -N (\d+) -n \d+ ln_exe/(.+)",
        ["node_num", "task"],
        name = "srun"
    )

    node1 = ParserNode(
        '''req payload size is (\d+) Byte
           .+ overhead is (.+) us \(total .+ s\)
           Total single-direction node bandwidth \(req/gross\): (.+) MB/s
           Total single-direction node bandwidth \(req/pure\): (.+) MB/s''',
        ["payload_size", "overhead", "gross_bw", "pure_bw"],
        name="arl"
    )

    node2 = ParserNode(
        '''req payload size = (\d+) Byte
           aggr_store overhead is (.+) us \(total .+ s\)
           Total single-direction node bandwidth \(req/pure\): (.+) MB/s''',
        ["payload_size", "overhead", "pure_bw"],
        name="upcxx"
    )

    node3 = ParserNode(
        '''Maximum medium payload size is (\d+)
           Node single-direction bandwidth = (.+) MB/S''',
        ["payload_size", "pure_bw"],
        name="gex"
    )

    node0.connect([node1, node2, node3])

    all_labels = ["node_num", "task", "payload_size", "overhead", "gross_bw", "pure_bw"]

    file_path = "./*.o*"
    lines = file_to_lines(file_path)

    graph = ParserGraph(node0, all_labels)
    df = graph.parse(lines)

    df["node_num"] = df.apply(lambda x: x["node_num"] * 32, axis=1)
    df.rename(columns={"node_num": "core_num"}, inplace=True)

    df.sort_values(by=["core_num"], inplace=True)
    print(list(df.columns))
    print(df.shape)
    df.to_csv("kcount_200520.csv")
