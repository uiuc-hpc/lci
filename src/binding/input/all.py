# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

import sys, os
sys.path.append(os.path.dirname(__file__))
from .tools import *

runtime_attr = [
    # attr("bool", "use_reg_cache", default_value="LCI_USE_REG_CACHE_DEFAULT", comment="Whether to use the registration cache."),
    # attr("bool", "use_control_channel", default_value=0, comment="Whether to use the control channel."),
    attr("size_t", "packet_return_threshold", default_value=4096, comment="The threshold for returning packets to its original pool."),
    attr("int", "imm_nbits_tag", default_value=16, comment="The number of bits for the immediate data tag."),
    attr("int", "imm_nbits_rcomp", default_value=15, comment="The number of bits for the immediate data remote completion handle."),
    attr("uint64_t", "max_imm_tag", inout_trait="out", comment="The max tag that can be put into the immediate data field. It is also the max tag that can be used in put with remote notification."),
    attr("uint64_t", "max_imm_rcomp", inout_trait="out", comment="The max rcomp that can be put into the immediate data field. It is also the max rcomp that can be used in put with remote notification."),
    attr("uint64_t", "max_tag", inout_trait="out", comment="The max tag that can be used in all primitives but put with remote notificaiton."),
    attr("uint64_t", "max_rcomp", inout_trait="out", comment="The max rcomp that can be used in all primitives but put with remote notification."),
    attr("bool", "alloc_default_device", default_value=1, comment="Whether to allocate the default device."),
    attr("bool", "alloc_default_packet_pool", default_value=1, comment="Whether to allocate the default packet pool."),
    attr("bool", "alloc_default_matching_engine", default_value=1, comment="Whether to allocate the default matching engine."),
    attr_enum("rdv_protocol", enum_options=["write"], default_value="write", comment="The rendezvous protocol to use."),
]

def get_g_runtime_init_args(runtime_attr):
    g_runtime_init_args = []
    for attr in runtime_attr:
        if "out" not in attr["trait"]:
            g_runtime_init_args.append(attr_to_arg(attr))
    return g_runtime_init_args

input = [
# ##############################################################################
# # Global
# ##############################################################################
operation(
    "is_active",
    [
        return_val("bool", "active", comment="whether the runtime is active")
    ],
    doc = {
        "in_group": "LCI_SETUP",
        "brief": "Check whether LCI is active.",
        "details": "This function can be called at any time. LCI is active if there is at least one runtime being active (!runtime.is_empty())."
    }
),
operation(
    "get_rank", 
    [
        return_val("int", "rank", comment="the rank of the current process")
    ],
    doc = {
        "in_group": "LCI_SETUP",
        "brief": "Get the rank of the current process.",
        "details": "This function can only be called when LCI is active."
    }
),
operation(
    "get_nranks",
    [
        return_val("int", "nranks", comment="the number of ranks in the current application/job")
    ],
    doc = {
        "in_group": "LCI_SETUP",
        "brief": "Get the number of ranks in the current application/job.",
        "details": "This function can only be called when LCI is active."
    }
),
# ##############################################################################
# # Runtime
# ##############################################################################
resource_runtime := resource(
    "runtime", 
    runtime_attr,
    children=[
        "net_context",
        "device",
        "packet_pool",
        "matching_engine",
    ],
    doc = {
        "in_group": "LCI_SETUP",
        "brief": "The runtime object.",
    }
),
operation_alloc(resource_runtime, add_runtime_args=False, init_global=True),
operation_free(resource_runtime, add_runtime_args=False, fina_global=True),
operation(
    "g_runtime_init", 
    get_g_runtime_init_args(runtime_attr),
    init_global=True,
    doc = {
        "in_group": "LCI_SETUP",
        "brief": "Initialize the global default runtime object.",
    }
),
operation(
    "g_runtime_fina", [],
    fina_global=True,
    doc = {
        "in_group": "LCI_SETUP",
        "brief": "Finalize the global default runtime object.",
    }
),
operation(
    "get_g_runtime",
    [
        return_val("runtime_t", "runtime", comment="The global default runtime object.")
    ],
    doc = {
        "in_group": "LCI_SETUP",
        "brief": "Get the global default runtime object.",
    }
),
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
        attr_enum("ibv_td_strategy", enum_options=["none", "all_qp", "per_qp"], default_value="per_qp", comment="For the IBV backend: the thread domain strategy."),
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
        optional_arg("device_t", "device", default_value="runtime.get_impl()->default_device", comment="The device to poll the network completion queue."),
        optional_arg("int", "max_polls", default_value=20, comment="The maximum number of completion descriptors to poll."),
        return_val("std::vector<net_status_t>", "statuses", comment="The polled completion descriptors.")
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
# ##############################################################################
# # Core Layer
# ##############################################################################
# packet pool
resource_packet_pool := resource(
    "packet_pool", 
    [
        attr("size_t", "packet_size", default_value="LCI_PACKET_SIZE_DEFAULT", comment="The size of the packet."),
        attr("size_t", "npackets", default_value="LCI_PACKET_NUM_DEFAULT", comment="The number of packets in the pool."),
        attr("size_t", "pbuffer_size", inout_trait="out", comment="The size of the packet buffer."),
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "The packet pool resource.",
    }
),
operation_alloc(resource_packet_pool),
operation_free(resource_packet_pool),

operation(
    "register_packet_pool", 
    [
        positional_arg("packet_pool_t", "packet_pool", comment="The packet pool to register."),
        positional_arg("device_t", "device", comment="The device to register the packet pool."),
        optional_runtime_args,
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Register a packet pool to a device.",
        "details": "A packet pool can be registered to multiple devices. A device can register multiple packet pools. However, a device can only be bound to one packet pool."
    }
),
operation(
    "deregister_packet_pool",
    [
        positional_arg("packet_pool_t", "packet_pool", comment="The packet pool to register."),
        positional_arg("device_t", "device", comment="The device to deregister the packet pool."),
        optional_runtime_args,
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Deregister a packet pool from a device.",
    }
),
# comp
resource_comp := resource(
    "comp", 
    [
        attr_enum("comp_type", enum_options=["sync", "cq", "handler"], default_value="cq", comment="The completion object type.", inout_trait="out"),
        attr("int", "sync_threshold", default_value=1, comment="The threshold for sync (synchronizer).", inout_trait="out"),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "The completion object resource.",
    }
),
operation_free(resource_comp),
operation(
    "comp_signal", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The completion object to signal."),
        positional_arg("status_t", "status", comment="The status to signal."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Signal a completion object.",
    }
),
operation(
    "reserve_rcomps", 
    [
        optional_runtime_args,
        positional_arg("rcomp_t", "n", comment="The number of remote completion handler slots to reserve."),
        return_val("rcomp_t", "start", comment="The start of the reserved slots."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Reserve spaces for n remote completion handlers.",
    }
),
operation(
    "register_rcomp", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The completion object to register."),
        optional_arg("rcomp_t", "rcomp", "0", comment="The remote completion handler slot used to register."),
        return_val("rcomp_t", "rcomp_out", comment="The remote completion handler pointing to the completion object."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Register a completion object into a remote completion handler.",
    }
),
operation(
    "deregister_rcomp", 
    [
        optional_runtime_args,
        positional_arg("rcomp_t", "rcomp", comment="The remote completion handler to deregister."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Deregister a remote completion handler.",
    }
),
# sync
operation(
    "alloc_sync", 
    [
        optional_runtime_args,
        optional_arg("int", "threshold", 1, comment="The signaling threshold of the synchronizer."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context associated with this completion object."),
        return_val("comp_t", "comp", comment="The allocated synchronizer.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Allocate a synchronizer.",
    }
),
operation(
    "sync_test", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The synchronizer to test."),
        positional_arg("status_t*", "p_out", inout_trait="out", comment="The pointer to an status array of size `comp.get_attr_sync_threshold()` to hold the outputs (only valid if the function returns *true*)."),
        return_val("bool", "succeed", comment="Whether the synchronizer is ready.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Test a synchronizer.",
    }
),
operation(
    "sync_wait", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The synchronizer to wait."),
        positional_arg("status_t*", "p_out", inout_trait="out", comment="The pointer to an status array of size `comp.get_attr_sync_threshold()` to hold the outputs."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Wait for a synchronizer to be ready.",
    }
),
# cq
operation(
    "alloc_cq", 
    [
        optional_runtime_args,
        optional_arg("int", "default_length", "8192", comment="The default length of the completion queue."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context associated with this completion object."),
        return_val("comp_t", "comp", comment="The allocated completion queue.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Allocate a completion queue.",
    }
),
operation(
    "cq_pop", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The completion queue to pop."),
        return_val("status_t", "status", comment="The popped status.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Pop a status from a completion queue.",
        "details": "This function is a nonblocking operation. It can return a status with an error code of either *retry* or *ok*. Other fields of the status are only valid if the error code is *ok*."
    }
),
# handler
operation(
    "alloc_handler", 
    [
        optional_runtime_args,
        positional_arg("comp_handler_t", "handler", comment="The handler function."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context associated with this completion object."),
        return_val("comp_t", "comp", comment="The allocated completion handler.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Allocate a completion handler.",
    }
),
# matching engine
resource_matching_engine := resource(
    "matching_engine", 
    [
        attr_enum("matching_engine_type", enum_options=["queue", "map"], default_value="map", comment="The type of the matching engine."),
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "The matching engine resource.",
    }
),
operation_alloc(resource_matching_engine),
operation_free(resource_matching_engine),
# communicate
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
        optional_arg("mr_t", "mr", "mr_t()", comment="The registered memory region for the local buffer."),
        optional_arg("uintptr_t", "remote_buffer", "0", comment="The remote buffer base address."),
        optional_arg("rkey_t", "rkey", "0", comment="The remote key of the remote buffer."),
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
        optional_arg("rcomp_t", "remote_comp", "0", comment="The remote completion handler to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context associated with this operation."),
        optional_arg("buffers_t", "buffers", "buffers_t()", comment="The local buffers."),
        optional_arg("rbuffers_t", "rbuffers", "rbuffers_t()", comment="The remote buffers."),
        optional_arg("matching_policy_t", "matching_policy", "matching_policy_t::rank_tag", comment="The matching policy to use."),
        optional_arg("bool", "allow_ok", "true", comment="Whether to allow the *ok* error code."),
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
        optional_arg("mr_t", "mr", "mr_t()", comment="The registered memory region for the local buffer."),
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("buffers_t", "buffers", "buffers_t()", comment="The local buffers."),
        optional_arg("bool", "allow_ok", "true", comment="Whether to allow the *ok* error code."),
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
        optional_arg("mr_t", "mr", "mr_t()", comment="The registered memory region for the local buffer."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("buffers_t", "buffers", "buffers_t()", comment="The local buffers."),
        optional_arg("matching_policy_t", "matching_policy", "matching_policy_t::rank_tag", comment="The matching policy to use."),
        optional_arg("bool", "allow_ok", "true", comment="Whether to allow the *ok* error code."),
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
        optional_arg("mr_t", "mr", "mr_t()", comment="The registered memory region for the local buffer."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("buffers_t", "buffers", "buffers_t()", comment="The local buffers."),
        optional_arg("matching_policy_t", "matching_policy", "matching_policy_t::rank_tag", comment="The matching policy to use."),
        optional_arg("bool", "allow_ok", "true", comment="Whether to allow the *ok* error code."),
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
        positional_arg("uintptr_t", "remote_buffer", "0", comment="The remote buffer base address."),
        positional_arg("rkey_t", "rkey", "0", comment="The remote key of the remote buffer."),
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to use."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("comp_semantic_t", "comp_semantic", "comp_semantic_t::buffer", comment="The completion semantic."),
        optional_arg("mr_t", "mr", "mr_t()", comment="The registered memory region for the local buffer."),
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
        optional_arg("rcomp_t", "remote_comp", "0", comment="The remote completion handler to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("buffers_t", "buffers", "buffers_t()", comment="The local buffers."),
        optional_arg("rbuffers_t", "rbuffers", "rbuffers_t()", comment="The remote buffers."),
        optional_arg("bool", "allow_ok", "true", comment="Whether to allow the *ok* error code."),
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
        positional_arg("uintptr_t", "remote_buffer", "0", comment="The remote buffer base address."),
        positional_arg("rkey_t", "rkey", "0", comment="The remote key of the remote buffer."),
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to use."),
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to use."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use."),
        optional_arg("mr_t", "mr", "mr_t()", comment="The registered memory region for the local buffer."),
        optional_arg("tag_t", "tag", "0", comment="The tag to use."),
        optional_arg("rcomp_t", "remote_comp", "0", comment="The remote completion handler to use."),
        optional_arg("void*", "user_context", "nullptr", comment="The arbitrary user-defined context associated with this operation."),
        optional_arg("buffers_t", "buffers", "buffers_t()", comment="The local buffers."),
        optional_arg("rbuffers_t", "rbuffers", "rbuffers_t()", comment="The remote buffers."),
        optional_arg("bool", "allow_ok", "true", comment="Whether to allow the *ok* error code."),
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
# progress
operation(
    "progress", 
    [
        optional_runtime_args,
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to progress."),
        optional_arg("endpoint_t", "endpoint", "device.get_impl()->default_endpoint", comment="The endpoint to use in case the progress function needs to send internal control messages."),
        return_val("error_t", "error", comment="The error code. The error code *ok* means the progress function progressed some work; the error code *retry* means the progress function did no find any work to progress."),
    ],
    doc = {
        "in_group": "LCI_COMM",
        "brief": "Perform background work to advance the state of the pending communication.",
    }
),
# ##############################################################################
# # Helper functions
# ##############################################################################
operation(
    "get_default_net_context", 
    [
        optional_runtime_args,
        return_val("net_context_t", "net_context", comment="The default net context.")
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Get the default net context of the runtime.",
    }
),
operation(
    "get_default_device", 
    [
        optional_runtime_args,
        return_val("device_t", "device", comment="The default net device.")
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Get the default net device of the runtime.",
    }
),
operation(
    "get_default_endpoint", 
    [
        optional_runtime_args,
        optional_arg("device_t", "device", "runtime.get_impl()->default_device", comment="The device to get the default endpoint."),
        return_val("endpoint_t", "endpoint", comment="The default net endpoint.")
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Get the default net endpoint of the runtime.",
    }
),
operation(
    "get_default_packet_pool", 
    [
        optional_runtime_args,
        return_val("packet_pool_t", "packet_pool", comment="The default packet pool.")
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Get the default packet pool of the runtime.",
    }
),
operation(
    "get_default_matching_engine", 
    [
        optional_runtime_args,
        return_val("matching_engine_t", "matching_engine", comment="The default matching engine.")
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Get the default matching engine of the runtime.",
    }
),
operation(
    "get_max_bcopy_size", 
    [
        optional_runtime_args,
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool that will be used."),
        return_val("size_t", "size", comment="Get the maximum message size for the buffer-copy protocol.")
    ],
    doc = {
        "in_group": "LCI_COMM",
        "brief": "Get the maximum message size for the buffer-copy protocol."
    }
),
# ##############################################################################
# # End of the definition
# ##############################################################################
]

def get_input():
    return input