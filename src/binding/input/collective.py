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
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
        optional_arg("comp_semantic_t", "comp_semantic", "comp_semantic_t::buffer", comment="The completion semantic."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "A blocking barrier operation.",
    }
),
operation(
    "broadcast", 
    [
        optional_runtime_args,
        positional_arg("int", "root", comment="The rank of the broadcast root."),
        positional_arg("void*", "buffer", comment="The local buffer base address."),
        positional_arg("size_t", "size", comment="The message size."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_coll_matching_engine", comment="The matching engine to use."),
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
    ],
    doc = {
        "in_group": "LCI_COLL",
        "brief": "A blocking barrier operation.",
    }
),
]

def get_input():
    return input