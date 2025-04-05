# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

import sys, os
sys.path.append(os.path.dirname(__file__))
from .tools import *

input = [
operation(
    "barrier", 
    [
        optional_runtime_args,
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_coll_matching_engine", comment="The matching engine to use."),
        optional_arg("comp_semantic_t", "comp_semantic", "comp_semantic_t::buffer", comment="The completion semantic."),
        optional_arg("comp_t", "comp", "comp_t()", comment="The completion to signal when the operation completes."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "A barrier operation.",
        "details": "Collective communication must be launched sequentially."
    }
),
operation(
    "broadcast", 
    [
        optional_runtime_args,
        positional_arg("void*", "buffer", comment="The local buffer base address."),
        positional_arg("size_t", "size", comment="The message size."),
        positional_arg("int", "root", comment="The rank of the broadcast root."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_coll_matching_engine", comment="The matching engine to use."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "A blocking broadcast operation.",
        "details": "Collective communication must be launched sequentially."
    }
),
operation(
    "reduce", 
    [
        optional_runtime_args,
        positional_arg("const void*", "sendbuf", comment="The local buffer base address to send."),
        positional_arg("void*", "recvbuf", comment="The local buffer base address to recv."),
        positional_arg("size_t", "count", comment="The number of data items in the buffer."),
        positional_arg("size_t", "item_size", comment="The size of each data item."),
        positional_arg("reduce_op_t", "op", comment="The reduction operation."),
        positional_arg("int", "root", comment="The rank of the broadcast root."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_coll_matching_engine", comment="The matching engine to use."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "A blocking reduce operation.",
        "details": "Collective communication must be launched sequentially."
    }
),
operation(
    "alltoall", 
    [
        optional_runtime_args,
        positional_arg("const void*", "sendbuf", comment="The local buffer base address to send."),
        positional_arg("void*", "recvbuf", comment="The local buffer base address to recv."),
        positional_arg("size_t", "size", comment="The message size per rank."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_coll_matching_engine", comment="The matching engine to use."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "A blocking alltoall operation.",
        "details": "Collective communication must be launched sequentially."
    }
),
]

def get_input():
    return input