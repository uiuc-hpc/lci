# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: NCSA

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
]

input = [
# ##############################################################################
# # Global
# ##############################################################################
operation(
    "get_rank_me", 
    [
        return_val("int", "rank_me", comment="the rank of the current process")
    ],
    doc = {
        "in_group": "LCI_SETUP",
        "brief": "Get the rank of the current process.",
        "details": "This function can only be called if there is at least one runtime being active (!runtime.is_empty())."
    }
),
operation(
    "get_rank_n",
    [
        return_val("int", "rank_n", comment="the number of ranks in the current application/job")
    ],
    doc = {
        "in_group": "LCI_SETUP",
        "brief": "Get the number of ranks in the current application/job.",
        "details": "This function can only be called if there is at least one runtime being active (!runtime.is_empty())."
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
op_alloc_runtime := operation_alloc(resource_runtime, add_runtime_args=False, init_global=True),
operation_free(resource_runtime, add_runtime_args=False, fina_global=True),
operation(
    "g_runtime_init",
    op_alloc_runtime["args"],
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
# # Core Layer
# ##############################################################################
# packet pool
resource_packet_pool := resource(
    "packet_pool", 
    [
        attr("size_t", "packet_size", default_value="LCI_PACKET_SIZE_DEFAULT", comment="The size of the packet."),
        attr("size_t", "npackets", default_value="LCI_PACKET_NUM_DEFAULT", comment="The number of packets in the pool."),
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
operation(
    "get_upacket",
    [
        optional_runtime_args,
        optional_arg("packet_pool_t", "packet_pool", "runtime.get_impl()->default_packet_pool", comment="The packet pool to get the packet."),
        return_val("void*", "packet", comment="The packet to use."),
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Get a packet for writing user payload from the packet pool.",
        "details": "The packet will be automatically returned to the packet pool when the communication is done."
    }
),
operation(
    "put_upacket",
    [
        optional_runtime_args,
        positional_arg("void*", "packet", comment="The packet to return."),
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Return a packet to the packet pool.",
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
operation(
    "matching_engine_insert",
    [
        optional_runtime_args,
        positional_arg("matching_engine_t", "matching_engine", comment="The matching engine to use."),
        positional_arg("matching_entry_key_t", "key", comment="The key to insert."),
        positional_arg("matching_entry_val_t", "value", comment="The value to insert."),
        positional_arg("matching_entry_type_t", "entry_type", comment="The type of the entry."),
        return_val("matching_entry_val_t", "entry", comment="The entry matched."),
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Insert an entry into the matching engine.",
    }
),
# ##############################################################################
# # Helper functions
# ##############################################################################
operation(
    "set_allocator",
    [
        optional_runtime_args,
        positional_arg("allocator_base_t*", "allocator", "nullptr", comment="The allocator to set as the allocator."),
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Set the message allocator for the runtime.",
        "details": "The default allocator will be used for all message buffers delivered to users."
    }
),
operation(
    "get_allocator",
    [
        optional_runtime_args,
        return_val("allocator_base_t*", "allocator", comment="The allocator used by the runtime."),
    ],
    doc = {
        "in_group": "LCI_RESOURCE",
        "brief": "Get the message allocator for the runtime.",
    }
),
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