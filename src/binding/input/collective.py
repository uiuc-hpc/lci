# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: NCSA

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
        optional_arg("comp_semantic_t", "comp_semantic", "comp_semantic_t::memory", comment="The completion semantic."),
        optional_arg("comp_t", "comp", "COMP_NULL", comment="The completion to signal when the operation completes."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "A barrier operation.",
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
        optional_arg("comp_t", "comp", "COMP_NULL", comment="The completion to signal when the operation completes."),
        optional_arg("broadcast_algorithm_t", "algorithm", "broadcast_algorithm_t::none", comment="The collective algorithm to use."),
        optional_arg("int", "ring_nsteps", "get_rank_n() - 1", comment="The number of steps in the ring algorithm."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "A broadcast operation.",
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
    }
),
operation(
    "reduce_scatter", 
    [
        optional_runtime_args,
        positional_arg("const void*", "sendbuf", comment="The local buffer base address to send."),
        positional_arg("void*", "recvbuf", comment="The local buffer base address to recv."),
        positional_arg("size_t", "recvcount", comment="The number of data items to receive one each rank."),
        positional_arg("size_t", "item_size", comment="The size of each data item."),
        positional_arg("reduce_op_t", "op", comment="The reduction operation."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_coll_matching_engine", comment="The matching engine to use."),
        optional_arg("comp_t", "comp", "COMP_NULL", comment="The completion to signal when the operation completes."),
        optional_arg("reduce_scatter_algorithm_t", "algorithm", "reduce_scatter_algorithm_t::none", comment="The collective algorithm to use."),
        optional_arg("int", "ring_nsteps", "get_rank_n() - 1", comment="The number of steps in the ring algorithm."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "A reduce scatter operation.",
        "details": "This operation assumes the send count is equal to `recvcount * item_size` and "
                   "`sendbuf` is of size at least `recvcount * item_size * get_rank_n()`.",
    }
),
operation(
    "allreduce", 
    [
        optional_runtime_args,
        positional_arg("const void*", "sendbuf", comment="The local buffer base address to send."),
        positional_arg("void*", "recvbuf", comment="The local buffer base address to recv."),
        positional_arg("size_t", "count", comment="The number of data items in the buffer."),
        positional_arg("size_t", "item_size", comment="The size of each data item."),
        positional_arg("reduce_op_t", "op", comment="The reduction operation."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_coll_matching_engine", comment="The matching engine to use."),
        optional_arg("comp_t", "comp", "COMP_NULL", comment="The completion to signal when the operation completes."),
        optional_arg("allreduce_algorithm_t", "algorithm", "allreduce_algorithm_t::none", comment="The collective algorithm to use."),
        optional_arg("int", "ring_nsteps", "get_rank_n() - 1", comment="The number of steps in the ring algorithm."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "An allreduce operation.",
    }
),
operation(
    "allgather", 
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
        "brief": "An allgather operation.",
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
    }
),
]

def get_input():
    return input