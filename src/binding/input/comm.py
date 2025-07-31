# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

import sys, os
sys.path.append(os.path.dirname(__file__))
from .tools import *

input = [
operation(
    "post_comm", 
    [
        optional_runtime_args,
        positional_arg("direction_t", "direction", comment="The communication direction."),
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "local_buffer", comment="The local buffer base address."),
        positional_arg("size_t", "size", comment="The message size."),
        positional_arg("comp_t", "local_comp", comment="The local completion object."),
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to use."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_matching_engine", comment="The matching engine to use."),
        optional_arg("comp_semantic_t", "comp_semantic", "comp_semantic_t::buffer", comment="The completion semantic (only valid when `direction == direction_t::OUT`)."),
        optional_arg("mr_t", "mr", "MR_HOST", comment="The registered memory region for the local buffer."),
        optional_arg("uintptr_t", "remote_disp", "0", comment="The displacement from the remote buffer base address."),
        optional_arg("rmr_t", "rmr", "RMR_NULL", comment="The remote memory region handle of the remote buffer."),
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
        optional_arg("rcomp_t", "remote_comp", "0", comment="The remote completion handler to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("matching_policy_t", "matching_policy", "matching_policy_t::rank_tag", comment="The matching policy to use."),
        optional_arg("bool", "allow_done", "true", comment="Whether to allow the *done* error code."),
        optional_arg("bool", "allow_posted", "true", comment="Whether to allow the *posted* error code."),
        optional_arg("bool", "allow_retry", "true", comment="Whether to allow the *retry* error code."),
        optional_arg("bool", "force_zcopy", "false", comment="Whether to force the zero-copy transfer."),
        return_val("status_t", "status", comment="The status of the operation."),
    ],
    doc = {
        "in_group": "LCI_COMM",
        "brief": "Post a generic communication operation.",
    }
),
operation(
    "post_am", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "local_buffer", comment="The local buffer base address."),
        positional_arg("size_t", "size", comment="The message size."),
        positional_arg("comp_t", "local_comp", comment="The local completion object."),
        positional_arg("rcomp_t", "remote_comp", comment="The remote completion handler to use."),
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to use."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("comp_semantic_t", "comp_semantic", "comp_semantic_t::buffer", comment="The completion semantic."),
        optional_arg("mr_t", "mr", "MR_HOST", comment="The registered memory region for the local buffer."),
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("bool", "allow_done", "true", comment="Whether to allow the *done* error code."),
        optional_arg("bool", "allow_posted", "true", comment="Whether to allow the *posted* error code."),
        optional_arg("bool", "allow_retry", "true", comment="Whether to allow the *retry* error code."),
        optional_arg("bool", "force_zcopy", "false", comment="Whether to force the zero-copy transfer."),
        return_val("status_t", "status", comment="The status of the operation."),
    ],
    doc = {
        "in_group": "LCI_COMM",
        "brief": "Post an active message communication operation.",
    }
),
operation(
    "post_send", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "local_buffer", comment="The local buffer base address."),
        positional_arg("size_t", "size", comment="The message size."),
        positional_arg("tag_t", "tag", comment="The tag to use."),
        positional_arg("comp_t", "local_comp", comment="The local completion object."),
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to use."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_matching_engine", comment="The matching engine to use."),
        optional_arg("comp_semantic_t", "comp_semantic", "comp_semantic_t::buffer", comment="The completion semantic."),
        optional_arg("mr_t", "mr", "MR_HOST", comment="The registered memory region for the local buffer."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("matching_policy_t", "matching_policy", "matching_policy_t::rank_tag", comment="The matching policy to use."),
        optional_arg("bool", "allow_done", "true", comment="Whether to allow the *done* error code."),
        optional_arg("bool", "allow_posted", "true", comment="Whether to allow the *posted* error code."),
        optional_arg("bool", "allow_retry", "true", comment="Whether to allow the *retry* error code."),
        optional_arg("bool", "force_zcopy", "false", comment="Whether to force the zero-copy transfer."),
        return_val("status_t", "status", comment="The status of the operation."),
    ],
    doc = {
        "in_group": "LCI_COMM",
        "brief": "Post a send communication operation.",
    }
),
operation(
    "post_recv", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The source rank."),
        positional_arg("void*", "local_buffer", comment="The local buffer base address."),
        positional_arg("size_t", "size", comment="The message size."),
        positional_arg("tag_t", "tag", comment="The tag to use."),
        positional_arg("comp_t", "local_comp", comment="The local completion object."),
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to use."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("matching_engine_t", "matching_engine", "runtime.get_impl()->default_matching_engine", comment="The matching engine to use."),
        optional_arg("mr_t", "mr", "MR_HOST", comment="The registered memory region for the local buffer."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("matching_policy_t", "matching_policy", "matching_policy_t::rank_tag", comment="The matching policy to use."),
        optional_arg("bool", "allow_done", "true", comment="Whether to allow the *done* error code."),
        optional_arg("bool", "allow_posted", "true", comment="Whether to allow the *posted* error code."),
        optional_arg("bool", "allow_retry", "true", comment="Whether to allow the *retry* error code."),
        optional_arg("bool", "force_zcopy", "false", comment="Whether to force the zero-copy transfer."),
        return_val("status_t", "status", comment="The status of the operation."),
    ],
    doc = {
        "in_group": "LCI_COMM",
        "brief": "Post a receive communication operation.",
    }
),
operation(
    "post_put", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "local_buffer", comment="The local buffer base address."),
        positional_arg("size_t", "size", comment="The message size."),
        positional_arg("comp_t", "local_comp", comment="The local completion object."),
        positional_arg("uintptr_t", "remote_disp", "0", comment="The displacement from the remote buffer base address."),
        positional_arg("rmr_t", "rmr", "RMR_NULL", comment="The remote memory region handle of the remote buffer."),
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to use."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("comp_semantic_t", "comp_semantic", "comp_semantic_t::buffer", comment="The completion semantic."),
        optional_arg("mr_t", "mr", "MR_HOST", comment="The registered memory region for the local buffer."),
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
        optional_arg("rcomp_t", "remote_comp", "0", comment="The remote completion handler to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("bool", "allow_done", "true", comment="Whether to allow the *done* error code."),
        optional_arg("bool", "allow_posted", "true", comment="Whether to allow the *posted* error code."),
        optional_arg("bool", "allow_retry", "true", comment="Whether to allow the *retry* error code."),
        optional_arg("bool", "force_zcopy", "false", comment="Whether to force the zero-copy transfer."),
        return_val("status_t", "status", comment="The status of the operation."),
    ],
    doc = {
        "in_group": "LCI_COMM",
        "brief": "Post a put (one-sided write) communication operation.",
    }
),
operation(
    "post_get", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "local_buffer", comment="The local buffer base address."),
        positional_arg("size_t", "size", comment="The message size."),
        positional_arg("comp_t", "local_comp", comment="The local completion object."),
        positional_arg("uintptr_t", "remote_disp", "0", comment="The displacement from the remote buffer base address."),
        positional_arg("rmr_t", "rmr", "RMR_NULL", comment="The remote memory region handle of the remote buffer."),
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to use."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("mr_t", "mr", "MR_HOST", comment="The registered memory region for the local buffer."),
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
        optional_arg("rcomp_t", "remote_comp", "0", comment="The remote completion handler to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("bool", "allow_done", "true", comment="Whether to allow the *done* error code."),
        optional_arg("bool", "allow_posted", "true", comment="Whether to allow the *posted* error code."),
        optional_arg("bool", "allow_retry", "true", comment="Whether to allow the *retry* error code."),
        optional_arg("bool", "force_zcopy", "false", comment="Whether to force the zero-copy transfer."),
        return_val("status_t", "status", comment="The status of the operation."),
    ],
    doc = {
        "in_group": "LCI_COMM",
        "brief": "Post a get (one-sided read) communication operation.",
    }
),
operation(
    "progress", 
    [
        optional_runtime_args,
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to progress."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use in case the progress function needs to send internal control messages."),
        return_val("error_t", "error", comment="The error code. The error code *done* means the progress function progressed some work; the error code *retry* means the progress function did no find any work to progress."),
    ],
    doc = {
        "in_group": "LCI_COMM",
        "brief": "Perform background work to advance the state of the pending communication.",
    }
),
]

def get_input():
    return input