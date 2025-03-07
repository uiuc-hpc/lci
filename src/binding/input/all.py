import sys, os
sys.path.append(os.path.dirname(__file__))
from tools import *

runtime_attr = [
    attr("bool", "use_reg_cache", default_value="LCI_USE_REG_CACHE_DEFAULT", comment="Whether to use the registration cache."),
    attr("bool", "use_control_channel", default_value=0, comment="Whether to use the control channel."),
    attr("int", "packet_return_threshold", default_value=4096, comment="The threshold for returning packets to its original pool."),
    attr("int", "imm_nbits_tag", default_value=16, comment="The number of bits for the immediate data tag."),
    attr("int", "imm_nbits_rcomp", default_value=15, comment="The number of bits for the immediate data remote completion handle."),
    attr("int", "max_imm_tag", inout_trait="out", comment="The max tag that can be put into the immediate data field."),
    attr("int", "max_imm_rcomp", inout_trait="out", comment="The max rcomp that can be put into the immediate data field."),
    attr("bool", "alloc_default_device", default_value=1, comment="Whether to allocate the default network device."),
    attr("bool", "alloc_default_packet_pool", default_value=1, comment="Whether to allocate the default packet pool."),
    attr_enum("rdv_protocol", enum_options=["write", "writeimm"], default_value="write", comment="The rendezvous protocol to use."),
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
    "get_rank", 
    [
        return_val("int", "rank", comment="the rank of the current process")
    ],
    comment = "Get the rank of the current process."
),
operation(
    "get_nranks",
    [
        return_val("int", "nranks", comment="the number of ranks in the current application/job")
    ],
    comment="Get the number of ranks in the current application/job."
),
# ##############################################################################
# # Runtime
# ##############################################################################
resource_runtime := resource(
    "runtime", 
    runtime_attr,
    comment="The runtime resource."
),
operation_alloc(resource_runtime, add_runtime_args=False, init_global=True),
operation_free(resource_runtime, add_runtime_args=False, fina_global=True),

operation(
    "g_runtime_init", 
    get_g_runtime_init_args(runtime_attr),
    init_global=True,
    comment="Initialize the global default runtime object."
),
operation(
    "g_runtime_fina", [],
    fina_global=True,
    comment="Finalize the global default runtime object."
),
operation("get_g_default_runtime", [return_val("runtime_t", "runtime")], comment="Get the global default runtime object."),
# ##############################################################################
# # Network Layer
# ##############################################################################
# net context
resource_net_context := resource(
    "net_context", 
    [
        attr("option_backend_t", "backend", comment="The network backend."),
        attr("std::string", "ofi_provider_name", default_value="LCI_OFI_PROVIDER_HINT_DEFAULT", comment="For the OFI backend: the provider name."),
        attr("int64_t", "max_msg_size", default_value="LCI_USE_MAX_SINGLE_MESSAGE_SIZE_DEFAULT", comment="The maximum message size."),
        attr("int64_t", "max_inject_size", default_value=64, comment="The maximum inject size."),
        attr("int", "ibv_gid_idx", default_value=-1, comment="For the IBV backend: the GID index by default (only needed by RoCE)."),
        attr("bool", "ibv_force_gid_auto_select", default_value=0, comment="For the IBV backend: whether to force GID auto selection."),
        attr_enum("ibv_odp_strategy", enum_options=["none", "explicit_odp", "implicit_odp"], default_value="none", comment="For the IBV backend: the on-demand paging strategy."),
        attr_enum("ibv_td_strategy", enum_options=["none", "all_qp", "per_qp"], default_value="per_qp", comment="For the IBV backend: the thread domain strategy."),
        attr_enum("ibv_prefetch_strategy", enum_options=["none", "prefetch", "prefetch_write", "prefetch_no_fault"], default_value="none", comment="For the IBV backend: the mr prefetch strategy."),
    ],
    comment="The network context resource."
),
operation_alloc(resource_net_context),
operation_free(resource_net_context),

# net device
resource_device := resource(
    "device", 
    [
        attr("int64_t", "net_max_sends", default_value="LCI_BACKEND_MAX_SENDS_DEFAULT"),
        attr("int64_t", "net_max_recvs", default_value="LCI_BACKEND_MAX_RECVS_DEFAULT"),
        attr("int64_t", "net_max_cqes", default_value="LCI_BACKEND_MAX_CQES_DEFAULT"),
        attr("uint64_t", "ofi_lock_mode", comment="For the OFI backend: the lock mode for the network device."),
    ],
    comment="The network device resource."
),
operation_alloc(
    resource_device, 
    [
        optional_arg("net_context_t", "net_context", "runtime.p_impl->net_context")
    ]
),
operation_free(resource_device),
# memory region
# resource("mr", []),
operation(
    "register_memory", 
    [
        optional_runtime_args,
        optional_arg("device_t", "device", "runtime.p_impl->device"),
        positional_arg("void*", "address"),
        positional_arg("size_t", "size"),
        return_val("mr_t", "mr")
    ],
    comment="Register a memory region to a network device."
),
operation(
    "deregister_memory", 
    [
        optional_runtime_args,
        positional_arg("mr_t*", "mr", inout_trait="inout")
    ],
    comment="Deregister a memory region from a network device."
),
operation(
    "get_rkey", 
    [
        positional_arg("mr_t", "mr"),
        optional_runtime_args,
        return_val("rkey_t", "rkey")
    ],
    comment="Get the rkey of a memory region."
),
# net endpoint
resource_endpoint := resource(
    "endpoint", []
),
operation_alloc(
    resource_endpoint, 
    [
        optional_arg("device_t", "device", "runtime.p_impl->device")
    ]
),
operation_free(resource_endpoint),
# operation
operation(
    "net_poll_cq", 
    [
        optional_runtime_args,
        optional_arg("device_t", "device", "runtime.p_impl->device"),
        optional_arg("int", "max_polls", 20),
        return_val("std::vector<net_status_t>", "statuses")
    ],
    comment="Poll the netowrk completion queue."
),
operation(
    "net_post_recv", 
    [
        optional_runtime_args,
        positional_arg("void*", "buffer"),
        positional_arg("size_t", "size"),
        positional_arg("mr_t", "mr"),
        optional_arg("device_t", "device", "runtime.p_impl->device"),
        optional_arg("void*", "ctx", "nullptr"),
        return_val("error_t", "error")
    ],
    comment="Post a network receive operation."
),
operation(
    "net_post_sends", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "buffer"),
        positional_arg("size_t", "size"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("net_imm_data_t", "imm_data", "0"),
        return_val("error_t", "error")
    ],
    comment="Post a network short send operation."
),
operation(
    "net_post_send", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "buffer"),
        positional_arg("size_t", "size"),
        positional_arg("mr_t", "mr"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("net_imm_data_t", "imm_data", "0"),
        optional_arg("void*", "ctx", "nullptr"),
        return_val("error_t", "error")
    ],
    comment="Post a network send operation."
),
operation(
    "net_post_puts", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "buffer"),
        positional_arg("size_t", "size"),
        positional_arg("uintptr_t", "base"),
        positional_arg("uint64_t", "offset"),
        positional_arg("rkey_t", "rkey"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        return_val("error_t", "error"),
    ],
    comment="Post a network short put operation."
),
operation(
    "net_post_put", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "buffer"),
        positional_arg("size_t", "size"),
        positional_arg("mr_t", "mr"),
        positional_arg("uintptr_t", "base"),
        positional_arg("uint64_t", "offset"),
        positional_arg("rkey_t", "rkey"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("void*", "ctx", "nullptr"),
        return_val("error_t", "error"),
    ],
    comment="Post a network put operation."
),
operation(
    "net_post_putImms", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "buffer"),
        positional_arg("size_t", "size"),
        positional_arg("uintptr_t", "base"),
        positional_arg("uint64_t", "offset"),
        positional_arg("rkey_t", "rkey"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("net_imm_data_t", "imm_data", "0"),
        return_val("error_t", "error"),
    ],
    comment="Post a network short put with immediate data operation."
),
operation(
    "net_post_putImm", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "buffer"),
        positional_arg("size_t", "size"),
        positional_arg("mr_t", "mr"),
        positional_arg("uintptr_t", "base"),
        positional_arg("uint64_t", "offset"),
        positional_arg("rkey_t", "rkey"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("net_imm_data_t", "imm_data", "0"),
        optional_arg("void*", "ctx", "nullptr"),
        return_val("error_t", "error"),
    ],
    comment="Post a network put with immediate data operation."
),
operation(
    "net_post_get", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "buffer"),
        positional_arg("size_t", "size"),
        positional_arg("mr_t", "mr"),
        positional_arg("uintptr_t", "base"),
        positional_arg("uint64_t", "offset"),
        positional_arg("rkey_t", "rkey"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("void*", "ctx", "nullptr"),
        return_val("error_t", "error"),
    ],
    comment="Post a network put operation."
),
# ##############################################################################
# # Core Layer
# ##############################################################################
# packet pool
resource_packet_pool := resource(
    "packet_pool", 
    [
        attr("size_t", "packet_size", default_value="LCI_PACKET_SIZE_DEFAULT"),
        attr("size_t", "npackets", default_value="LCI_PACKET_NUM_DEFAULT"),
        attr("size_t", "pbuffer_size", inout_trait="out", comment="The size of the packet buffer."),
    ],
    comment="The packet pool resource."
),
operation_alloc(resource_packet_pool),
operation_free(resource_packet_pool),

operation(
    "bind_packet_pool", 
    [
        positional_arg("device_t", "device"),
        positional_arg("packet_pool_t", "packet_pool"),
        optional_runtime_args,
    ],
    comment="""Bind a packet pool to a network device.
This is only needed for explicit packet pool.
Implicit packet pool (the one allocated by the runtime) 
is automatically bound to all network device.
"""
),
operation(
    "unbind_packet_pool",
    [
        positional_arg("device_t", "device"),
        optional_runtime_args,
    ],
    comment="Unbind the packet pool from a network device."
),
# comp
resource_comp := resource(
    "comp", 
    [
        attr_enum("comp_type", enum_options=["sync", "cq", "handler"], default_value="cq", comment="The completion object type.", inout_trait="out"),
        attr("int", "sync_threshold", default_value=1, comment="The threshold for sync (synchronizer).", inout_trait="out"),
    ]
),
operation_free(resource_comp),
operation(
    "comp_signal", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp"),
        positional_arg("status_t", "status")
    ],
    comment="Signal a completion object."
),
operation(
    "reserve_rcomps", 
    [
        optional_runtime_args,
        positional_arg("rcomp_t", "n"),
        return_val("rcomp_t", "start")
    ],
    comment="Reserve spaces for n remote completion handlers."
),
operation(
    "register_rcomp", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp"),
        optional_arg("rcomp_t", "rcomp", "0"),
        return_val("rcomp_t", "rcomp_out")
    ],
    comment="Register a completion object into a remote completion handler."
),
operation(
    "deregister_rcomp", 
    [
        optional_runtime_args,
        positional_arg("rcomp_t", "rcomp"),
    ],
    comment="Deregister a remote completion handler."
),
# sync
operation(
    "alloc_sync", 
    [
        optional_runtime_args,
        optional_arg("int", "threshold", 1),
        optional_arg("void*", "user_context", "nullptr"),
        return_val("comp_t", "comp")
    ],
    comment="Allocate a synchronizer."
),
operation(
    "sync_test", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp"),
        positional_arg("status_t*", "p_out", inout_trait="out"),
        return_val("bool", "succeed")
    ],
    comment="Test a synchronizer."
),
operation(
    "sync_wait", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp"),
        positional_arg("status_t*", "p_out", inout_trait="out"),
    ],
    comment="Wait a synchronizer to be ready."
),
# cq
operation(
    "alloc_cq", 
    [
        optional_runtime_args,
        optional_arg("int", "default_length", "8192"),
        optional_arg("void*", "user_context", "nullptr"),
        return_val("comp_t", "comp")
    ],
    comment="Allocate a completion queue."
),
operation(
    "cq_pop", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp"),
        return_val("status_t", "status")
    ]
),
# handler
operation(
    "alloc_handler", 
    [
        optional_runtime_args,
        positional_arg("comp_handler_t", "handler"),
        optional_arg("void*", "user_context", "nullptr"),
        return_val("comp_t", "comp")
    ],
    comment="Allocate a completion handler."
),
# matching engine
resource_matching_engine := resource(
    "matching_engine", 
    [
        attr_enum("matching_engine_type", enum_options=["queue", "map"], default_value="map", comment="The type of the matching engine."),
    ]),
operation_alloc(resource_matching_engine),
operation_free(resource_matching_engine),
# communicate
operation(
    "post_comm", 
    [
        optional_runtime_args,
        positional_arg("direction_t", "direction"),
        positional_arg("int", "rank"),
        positional_arg("void*", "local_buffer"),
        positional_arg("size_t", "size"),
        positional_arg("comp_t", "local_comp"),
        optional_arg("packet_pool_t", "packet_pool", "runtime.p_impl->packet_pool"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("matching_engine_t", "matching_engine", "runtime.p_impl->matching_engine"),
        optional_arg("out_comp_type_t", "out_comp_type", "out_comp_type_t::buffer"),
        optional_arg("mr_t", "mr", "mr_t()"),
        optional_arg("uintptr_t", "remote_buffer", "0"),
        optional_arg("rkey_t", "rkey", "0"),
        optional_arg("tag_t", "tag", "0"),
        optional_arg("rcomp_t", "remote_comp", "0"),
        optional_arg("void*", "ctx", "nullptr"),
        optional_arg("buffers_t", "buffers", "buffers_t()"),
        optional_arg("rbuffers_t", "rbuffers", "rbuffers_t()"),
        optional_arg("matching_policy_t", "matching_policy", "matching_policy_t::rank_tag"),
        optional_arg("bool", "allow_ok", "true"),
        optional_arg("bool", "allow_retry", "true"),
        optional_arg("bool", "force_zero_copy", "false"),
        return_val("status_t", "status"),
    ]
),
operation(
    "post_am", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "local_buffer"),
        positional_arg("size_t", "size"),
        positional_arg("comp_t", "local_comp"),
        positional_arg("rcomp_t", "remote_comp"),
        optional_arg("packet_pool_t", "packet_pool", "runtime.p_impl->packet_pool"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("out_comp_type_t", "out_comp_type", "out_comp_type_t::buffer"),
        optional_arg("mr_t", "mr", "mr_t()"),
        optional_arg("tag_t", "tag", "0"),
        optional_arg("void*", "ctx", "nullptr"),
        optional_arg("buffers_t", "buffers", "buffers_t()"),
        optional_arg("bool", "allow_ok", "true"),
        optional_arg("bool", "allow_retry", "true"),
        optional_arg("bool", "force_zero_copy", "false"),
        return_val("status_t", "status"),
    ]
),
operation(
    "post_send", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "local_buffer"),
        positional_arg("size_t", "size"),
        positional_arg("tag_t", "tag"),
        positional_arg("comp_t", "local_comp"),
        optional_arg("packet_pool_t", "packet_pool", "runtime.p_impl->packet_pool"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("matching_engine_t", "matching_engine", "runtime.p_impl->matching_engine"),
        optional_arg("out_comp_type_t", "out_comp_type", "out_comp_type_t::buffer"),
        optional_arg("mr_t", "mr", "mr_t()"),
        optional_arg("void*", "ctx", "nullptr"),
        optional_arg("buffers_t", "buffers", "buffers_t()"),
        optional_arg("matching_policy_t", "matching_policy", "matching_policy_t::rank_tag"),
        optional_arg("bool", "allow_ok", "true"),
        optional_arg("bool", "allow_retry", "true"),
        optional_arg("bool", "force_zero_copy", "false"),
        return_val("status_t", "status"),
    ]
),
operation(
    "post_recv", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "local_buffer"),
        positional_arg("size_t", "size"),
        positional_arg("tag_t", "tag"),
        positional_arg("comp_t", "local_comp"),
        optional_arg("packet_pool_t", "packet_pool", "runtime.p_impl->packet_pool"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("matching_engine_t", "matching_engine", "runtime.p_impl->matching_engine"),
        optional_arg("mr_t", "mr", "mr_t()"),
        optional_arg("void*", "ctx", "nullptr"),
        optional_arg("buffers_t", "buffers", "buffers_t()"),
        optional_arg("matching_policy_t", "matching_policy", "matching_policy_t::rank_tag"),
        optional_arg("bool", "allow_ok", "true"),
        optional_arg("bool", "allow_retry", "true"),
        optional_arg("bool", "force_zero_copy", "false"),
        return_val("status_t", "status"),
    ]
),
operation(
    "post_put", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "local_buffer"),
        positional_arg("size_t", "size"),
        positional_arg("comp_t", "local_comp"),
        positional_arg("uintptr_t", "remote_buffer", "0"),
        positional_arg("rkey_t", "rkey", "0"),
        optional_arg("packet_pool_t", "packet_pool", "runtime.p_impl->packet_pool"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("out_comp_type_t", "out_comp_type", "out_comp_type_t::buffer"),
        optional_arg("mr_t", "mr", "mr_t()"),
        optional_arg("tag_t", "tag", "0"),
        optional_arg("rcomp_t", "remote_comp", "0"),
        optional_arg("void*", "ctx", "nullptr"),
        optional_arg("buffers_t", "buffers", "buffers_t()"),
        optional_arg("rbuffers_t", "rbuffers", "rbuffers_t()"),
        optional_arg("bool", "allow_ok", "true"),
        optional_arg("bool", "allow_retry", "true"),
        optional_arg("bool", "force_zero_copy", "false"),
        return_val("status_t", "status"),
    ]
),
operation(
    "post_get", 
    [
        optional_runtime_args,
        positional_arg("int", "rank"),
        positional_arg("void*", "local_buffer"),
        positional_arg("size_t", "size"),
        positional_arg("comp_t", "local_comp"),
        positional_arg("uintptr_t", "remote_buffer", "0"),
        positional_arg("rkey_t", "rkey", "0"),
        optional_arg("packet_pool_t", "packet_pool", "runtime.p_impl->packet_pool"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("mr_t", "mr", "mr_t()"),
        optional_arg("tag_t", "tag", "0"),
        optional_arg("rcomp_t", "remote_comp", "0"),
        optional_arg("void*", "ctx", "nullptr"),
        optional_arg("buffers_t", "buffers", "buffers_t()"),
        optional_arg("rbuffers_t", "rbuffers", "rbuffers_t()"),
        optional_arg("bool", "allow_ok", "true"),
        optional_arg("bool", "allow_retry", "true"),
        optional_arg("bool", "force_zero_copy", "false"),
        return_val("status_t", "status"),
    ]
),
# progress
operation(
    "progress", 
    [
        optional_runtime_args,
        optional_arg("device_t", "device", "runtime.p_impl->device"),
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        return_val("error_t", "error")
    ]
),
# ##############################################################################
# # Helper functions
# ##############################################################################
operation(
    "get_default_net_context", 
    [
        optional_runtime_args,
        return_val("net_context_t", "net_context")
    ],
    comment="Get the default net context."
),
operation(
    "get_default_device", 
    [
        optional_runtime_args,
        return_val("device_t", "device")
    ],
    comment="Get the default net device."
),
operation(
    "get_default_endpoint", 
    [
        optional_runtime_args,
        return_val("endpoint_t", "endpoint")
    ],
    comment="Get the default net endpoint."
),
operation(
    "get_default_packet_pool", 
    [
        optional_runtime_args,
        return_val("packet_pool_t", "packet_pool")
    ],
    comment="Get the default packet pool."
),
operation(
    "get_max_inject_size", 
    [
        optional_runtime_args,
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("tag_t", "tag", "0"),
        optional_arg("rcomp_t", "remote_comp", "0"),
        return_val("size_t", "size")
    ],
    comment="Get the maximum message size for the inject protocol."
),
operation(
    "get_max_bcopy_size", 
    [
        optional_runtime_args,
        optional_arg("endpoint_t", "endpoint", "runtime.p_impl->endpoint"),
        optional_arg("packet_pool_t", "packet_pool", "runtime.p_impl->packet_pool"),
        optional_arg("tag_t", "tag", "0"),
        optional_arg("rcomp_t", "remote_comp", "0"),
        return_val("size_t", "size")
    ],
    comment="Get the maximum message size for the eager protocol."
),
# ##############################################################################
# # End of the definition
# ##############################################################################
]

def get_input():
    return input