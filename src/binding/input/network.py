# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

import sys, os
sys.path.append(os.path.dirname(__file__))
from .tools import *

input = [
# ##############################################################################
# # Network Layer
# ##############################################################################
# net context
resource_net_context := resource(
    "net_context", 
    [
        attr("attr_backend_t", "backend", comment="The network backend to use."),
        attr("std::string", "ofi_provider_name", default_value="LCI_OFI_PROVIDER_HINT_DEFAULT", comment="For the OFI backend: the provider name."),
        attr("size_t", "max_msg_size", default_value="LCI_USE_MAX_SINGLE_MESSAGE_SIZE_DEFAULT", comment="The maximum message size."),
        attr("size_t", "max_inject_size", default_value=64, comment="The maximum inject size."),
        attr("int", "ibv_gid_idx", default_value=-1, comment="For the IBV backend: the GID index by default (only needed by RoCE)."),
        attr("bool", "ibv_force_gid_auto_select", default_value=0, comment="For the IBV backend: whether to force GID auto selection."),
        attr_enum("ibv_odp_strategy", enum_options=["none", "explicit_odp", "implicit_odp"], default_value="none", comment="For the IBV backend: the on-demand paging strategy."),
        attr_enum("ibv_prefetch_strategy", enum_options=["none", "prefetch", "prefetch_write", "prefetch_no_fault"], default_value="none", comment="For the IBV backend: the mr prefetch strategy."),
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "The network context resource.",
    }
),
operation_alloc(resource_net_context),
operation_free(resource_net_context),

# net device
resource_device := resource(
    "device", 
    [
        attr("size_t", "net_max_sends", default_value="LCI_BACKEND_MAX_SENDS_DEFAULT", comment="The maximum number of sends that can be posted to the underlying network queue at the same time."),
        attr("size_t", "net_max_recvs", default_value="LCI_BACKEND_MAX_RECVS_DEFAULT", comment="The maximum number of receives that can be posted to the underlying network queue at the same time."),
        attr("size_t", "net_max_cqes", default_value="LCI_BACKEND_MAX_CQES_DEFAULT", comment="The maximum number of CQEs that can reside in the underlying network queue at the same time."),
        attr("uint64_t", "ofi_lock_mode", comment="For the OFI backend: the lock mode for the device."),
        attr("bool", "alloc_default_endpoint", default_value=1, comment="Whether to allocate the default endpoint."),
        attr("int", "uid", default_value=-1, inout_trait="out", comment="A unique device id across the entire process."),
        attr_enum("ibv_td_strategy", enum_options=["none", "all_qp", "per_qp"], default_value="per_qp", comment="For the IBV backend: the thread domain strategy."),
    ],
    children=[
        "endpoint",
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "The device resource.",
    }
),
operation_alloc(
    resource_device, 
    [
        optional_arg("net_context_t", "net_context", "runtime.get_impl()->default_net_context", comment="The network context to allocate the device."),
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to bind."),
    ]
),
operation_free(resource_device),
# memory region
resource(
    "mr", 
    [],
    doc = {
        "in_group": "LCI_MEMORY",
        "brief": "The memory region resource.",
    }
),
operation(
    "register_memory", 
    [
        optional_runtime_args,
        optional_arg("device_t", "device", default_value="runtime.get_impl()->default_device", comment="The device to register the memory region."),
        positional_arg("void*", "address", comment="The base address of the memory region to register."),
        positional_arg("size_t", "size", comment="The size of the memory region to register."),
        return_val("mr_t", "mr", comment="The handler for the registered memory region.")
    ],
    doc = {
        "in_group": "LCI_MEMORY",
        "brief": "Register a memory region to a device.",
    }
),
operation(
    "deregister_memory", 
    [
        optional_runtime_args,
        positional_arg("mr_t*", "mr", inout_trait="inout", comment="The memory region to deregister.")
    ],
    doc = {
        "in_group": "LCI_MEMORY",
        "brief": "Deregister a memory region from a device.",
    }
),
operation(
    "get_rkey", 
    [
        positional_arg("mr_t", "mr", comment="The memory region to get the rkey."),
        optional_runtime_args,
        return_val("rkey_t", "rkey", comment="The rkey of the memory region.")
    ],
    doc = {
        "in_group": "LCI_MEMORY",
        "brief": "Get the rkey of a memory region.",
    }
),
# net endpoint
resource_endpoint := resource(
    "endpoint", 
    [
        attr("int", "uid", default_value=-1, inout_trait="out", comment="A unique endpoint id across the entire process."),
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "The endpoint resource.",
    }
),
operation_alloc(
    resource_endpoint, 
    [
        optional_arg("device_t", "device", default_value="runtime.get_impl()->default_device", comment="The device to allocate the endpoint.")
    ]
),
operation_free(resource_endpoint),
# operation
operation(
    "net_poll_cq", 
    [
        optional_runtime_args,
        positional_arg("size_t", "max_polls", comment="The maximum number of completion descriptors to poll."),
        positional_arg("net_status_t*", "statuses", inout_trait="out", comment="The output array for the completion descriptors (can be nullptr)."),
        optional_arg("device_t", "device", default_value="runtime.get_impl()->default_device", comment="The device to poll the network completion queue."),
        return_val("size_t", "n", comment="Number of completion descriptors polled from the network layer.")
    ],
    doc = {
        "in_group": "LCI_NET",
        "brief": "Poll the network completion queue.",
    }
),
operation(
    "net_post_recv", 
    [
        optional_runtime_args,
        positional_arg("void*", "buffer", comment="The receive buffer base address."),
        positional_arg("size_t", "size", comment="The receive buffer size."),
        positional_arg("mr_t", "mr", comment="The registered memory region for the receive buffer."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to post the receive operation."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context."),
        return_val("error_t", "error", comment="The error code.")
    ],
    doc = {
        "in_group": "LCI_NET",
        "brief": "Post a network receive operation.",
    }
),
operation(
    "net_post_sends", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "buffer", comment="The send buffer base address."),
        positional_arg("size_t", "size", comment="The send buffer size."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("net_imm_data_t", "imm_data", "0", comment="The immediate data to send with the message."),
        return_val("error_t", "error", comment="The error code.")
    ],
    doc = {
        "in_group": "LCI_NET",
        "brief": "Post a network short send operation.",
        "details": "This operation uses the *inject* semantics: the data is directly copied into the network hardware with the operation descriptor and the operation is completed immediately."
    }
),
operation(
    "net_post_send", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "buffer", comment="The send buffer base address."),
        positional_arg("size_t", "size", comment="The send buffer size."),
        positional_arg("mr_t", "mr", comment="The registered memory region for the send buffer."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("net_imm_data_t", "imm_data", "0", comment="The immediate data to send with the message."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context."),
        return_val("error_t", "error", comment="The error code.")
    ],
    doc = {
        "in_group": "LCI_NET",
        "brief": "Post a network send operation.",
        "details": "This operation uses the *post* semantics: means the operation is posted and not completed immediately; the completed operation will be reported through @ref net_poll_cq; the send buffer can only be written after the operation is completed."
    }
),
operation(
    "net_post_puts", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "buffer", comment="The send buffer base address."),
        positional_arg("size_t", "size", comment="The send buffer size."),
        positional_arg("uintptr_t", "base", comment="The base address of the remote region to put the data."),
        positional_arg("uint64_t", "offset", comment="The starting offset to the remote region base address to put the data."),
        positional_arg("rkey_t", "rkey", comment="The remote memory region key."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        return_val("error_t", "error", comment="The error code."),
    ],
    doc = {
        "in_group": "LCI_NET",
        "brief": "Post a network short put operation.",
        "details": "This operation uses the *inject* semantics: the data is directly copied into the network hardware with the operation descriptor and the operation is completed immediately."
    }
),
operation(
    "net_post_put", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "buffer", comment="The send buffer base address."),
        positional_arg("size_t", "size", comment="The send buffer size."),
        positional_arg("mr_t", "mr", comment="The registered memory region for the send buffer."),
        positional_arg("uintptr_t", "base", comment="The base address of the remote region to put the data."),
        positional_arg("uint64_t", "offset", comment="The starting offset to the remote region base address to put the data."),
        positional_arg("rkey_t", "rkey", comment="The remote memory region key."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context."),
        return_val("error_t", "error", comment="The error code."),
    ],
    doc = {
        "in_group": "LCI_NET",
        "brief": "Post a network put operation.",
        "details": "This operation uses the *post* semantics: means the operation is posted and not completed immediately; the completed operation will be reported through @ref net_poll_cq; the send buffer can only be written after the operation is completed."
    }
),
operation(
    "net_post_putImms", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "buffer", comment="The send buffer base address."),
        positional_arg("size_t", "size", comment="The send buffer size."),
        positional_arg("uintptr_t", "base", comment="The base address of the remote region to put the data."),
        positional_arg("uint64_t", "offset", comment="The starting offset to the remote region base address to put the data."),
        positional_arg("rkey_t", "rkey", comment="The remote memory region key."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("net_imm_data_t", "imm_data", "0", comment="The immediate data to put with the message."),
        return_val("error_t", "error", comment="The error code."),
    ],
    doc = {
        "in_group": "LCI_NET",
        "brief": "Post a network *short put with immediate data* operation.",
        "details": "This operation uses the *inject* semantics: the data is directly copied into the network hardware with the operation descriptor and the operation is completed immediately."
    }
),
operation(
    "net_post_putImm", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "buffer", comment="The send buffer base address."),
        positional_arg("size_t", "size", comment="The send buffer size."),
        positional_arg("mr_t", "mr", comment="The registered memory region for the send buffer."),
        positional_arg("uintptr_t", "base", comment="The base address of the remote region to put the data."),
        positional_arg("uint64_t", "offset", comment="The starting offset to the remote region base address to put the data."),
        positional_arg("rkey_t", "rkey", comment="The remote memory region key."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("net_imm_data_t", "imm_data", "0", comment="The immediate data to put with the message."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context."),
        return_val("error_t", "error", comment="The error code."),
    ],
    doc = {
        "in_group": "LCI_NET",
        "brief": "Post a network put with immediate data operation.",
        "details": "This operation uses the *post* semantics: means the operation is posted and not completed immediately; the completed operation will be reported through @ref net_poll_cq; the send buffer can only be written after the operation is completed."
    }
),
operation(
    "net_post_get", 
    [
        optional_runtime_args,
        positional_arg("int", "rank", comment="The target rank."),
        positional_arg("void*", "buffer", comment="The receive buffer base address."),
        positional_arg("size_t", "size", comment="The receive buffer size."),
        positional_arg("mr_t", "mr", comment="The registered memory region for the receive buffer."),
        positional_arg("uintptr_t", "base", comment="The base address of the remote region to get the data."),
        positional_arg("uint64_t", "offset", comment="The starting offset to the remote region base address to get the data."),
        positional_arg("rkey_t", "rkey", comment="The remote memory region key."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context."),
        return_val("error_t", "error", comment="The error code."),
    ],
    doc = {
        "in_group": "LCI_NET",
        "brief": "Post a network get operation.",
        "details": "This operation uses the *post* semantics: means the operation is posted and not completed immediately; the completed operation will be reported through @ref net_poll_cq; the receive buffer can only be read after the operation is completed."
    }
),
]

def get_input():
    return input