import sys, os
sys.path.append(os.path.dirname(__file__))
from tools import *

input = [
# ##############################################################################
# # Global
# ##############################################################################
operation("get_rank", [
    optional_error_args,
    positional_args("int*", "rank")
]),
operation("get_nranks", [
    optional_error_args,
    positional_args("int*", "nranks")
]),
# ##############################################################################
# # Runtime
# ##############################################################################
resource_runtime := resource("runtime", [
    attr("bool", "use_reg_cache", "LCIXX_USE_REG_CACHE_DEFAULT"),
    attr("bool", "use_control_channel", 1),
    attr("bool", "use_default_net_context", 1),
    attr("bool", "use_default_net_device", 1),
    attr("bool", "use_default_net_endpoint", 1),
    attr("option_rdv_protocol_t", "rdv_protocol") #TODO
]),
operation_alloc(resource_runtime, add_runtime_args=False),
operation_free(resource_runtime, add_runtime_args=False),
operation("g_runtime_init", [
    optional_error_args,
    optional_args("bool", "use_reg_cache"),
    optional_args("bool", "use_control_channel"),
    optional_args("bool", "use_default_net_context"),
    optional_args("bool", "use_default_net_device"),
    optional_args("option_rdv_protocol_t", "rdv_protocol")
]),
operation("g_runtime_fina", [
    optional_error_args,
]),
operation("get_default_net_context", [
    runtime_args,
    optional_error_args,
    positional_args("net_context_t*", "net_context")
]),
operation("get_default_net_device", [
    runtime_args,
    optional_error_args,
    positional_args("net_device_t*", "net_device")
]),
operation("get_default_net_endpoint", [
    runtime_args,
    optional_error_args,
    positional_args("net_endpoint_t*", "net_endpoint")
]),
# ##############################################################################
# # Network Layer
# ##############################################################################
# net context
resource_net_context := resource("net_context", [
    attr("option_backend_t", "backend"), #TODO
    attr("std::string", "provider_name"),
    attr("int64_t", "max_msg_size", "LCIXX_USE_MAX_SINGLE_MESSAGE_SIZE_DEFAULT"),
]),
operation_alloc(resource_net_context),
operation_free(resource_net_context),
# net device
resource_net_device := resource("net_device", [
    attr("int64_t", "max_sends", "LCIXX_BACKEND_MAX_SENDS_DEFAULT"),
    attr("int64_t", "max_recvs", "LCIXX_BACKEND_MAX_RECVS_DEFAULT"),
    attr("int64_t", "max_cqes", "LCIXX_BACKEND_MAX_CQES_DEFAULT"),
    attr("uint64_t", "lock_mode") #TODO
]),
operation_alloc(resource_net_device, [
    optional_args("net_context_t", "net_context")
]),
operation_free(resource_net_device),
# memory region
resource("mr", [
]),
operation("register_memory", [
    runtime_args,
    optional_error_args,
    optional_args("net_device_t", "net_device"),
    positional_args("void*", "address"),
    positional_args("size_t", "size"),
    positional_args("mr_t*", "mr")
]),
operation("deregister_memory", [
    runtime_args,
    optional_error_args,
    positional_args("mr_t*", "mr")
]),
operation("get_rkey", [
    runtime_args,
    optional_error_args,
    positional_args("rkey_t*", "rkey")
]),
# net endpoint
resource_net_endpoint := resource("net_endpoint", [
]),
operation_alloc(resource_net_endpoint, [
    optional_args("net_device_t", "net_device")
]),
operation_free(resource_net_endpoint),
# operation
operation("net_poll_cq", [
    runtime_args,
    optional_error_args,
    optional_args("net_device_t", "net_device"),
    optional_args("int", "max_polls"),
    positional_args("std::vector<net_status_t>*", "statuses")
]),
operation("net_post_recv", [
    runtime_args,
    optional_args("net_device_t", "net_device"),
    positional_args("void*", "buffer"),
    positional_args("size_t", "size"),
    positional_args("mr_t", "mr"),
    optional_args("void*", "ctx"),
    positional_error_args,
]),
operation("net_post_sends", [
    runtime_args,
    optional_args("net_endpoint_t", "net_endpoint"),
    positional_args("int", "rank"),
    positional_args("void*", "buffer"),
    positional_args("size_t", "size"),
    optional_args("net_imm_data_t", "imm_data"),
    positional_error_args,
]),
operation("net_post_send", [
    runtime_args,
    optional_args("net_endpoint_t", "net_endpoint"),
    positional_args("int", "rank"),
    positional_args("void*", "buffer"),
    positional_args("size_t", "size"),
    positional_args("mr_t", "mr"),
    optional_args("net_imm_data_t", "imm_data"),
    optional_args("void*", "ctx"),
    positional_error_args,
]),
operation("net_post_puts", [
    runtime_args,
    optional_args("net_endpoint_t", "net_endpoint"),
    positional_args("int", "rank"),
    positional_args("void*", "buffer"),
    positional_args("size_t", "size"),
    positional_args("uintptr_t", "base"),
    positional_args("uint64_t", "offset"),
    positional_args("rkey_t", "rkey"),
    positional_error_args,
]),
operation("net_post_put", [
    runtime_args,
    optional_args("net_endpoint_t", "net_endpoint"),
    positional_args("int", "rank"),
    positional_args("void*", "buffer"),
    positional_args("size_t", "size"),
    positional_args("mr_t", "mr"),
    positional_args("uintptr_t", "base"),
    positional_args("uint64_t", "offset"),
    positional_args("rkey_t", "rkey"),
    optional_args("void*", "ctx"),
    positional_error_args,
]),
operation("net_post_putImms", [
    runtime_args,
    optional_args("net_endpoint_t", "net_endpoint"),
    positional_args("int", "rank"),
    positional_args("void*", "buffer"),
    positional_args("size_t", "size"),
    positional_args("uintptr_t", "base"),
    positional_args("uint64_t", "offset"),
    positional_args("rkey_t", "rkey"),
    optional_args("net_imm_data_t", "imm_data"),
    positional_error_args,
]),
operation("net_post_putImm", [
    runtime_args,
    optional_args("net_endpoint_t", "net_endpoint"),
    positional_args("int", "rank"),
    positional_args("void*", "buffer"),
    positional_args("size_t", "size"),
    positional_args("mr_t", "mr"),
    positional_args("uintptr_t", "base"),
    positional_args("uint64_t", "offset"),
    positional_args("rkey_t", "rkey"),
    optional_args("net_imm_data_t", "imm_data"),
    optional_args("void*", "ctx"),
    positional_error_args,
]),
# ##############################################################################
# # Core Layer
# ##############################################################################
resource("packet_pool", []),
resource("comp", []),
operation("communicate", [
    runtime_args,
    positional_args("direction_t", "direction"),
    positional_args("int", "rank"),
    positional_args("void*", "local_buffer"),
    positional_args("size_t", "size"),
    positional_args("comp_t", "local_comp"),
    optional_args("net_endpoint_t", "net_endpoint"),
    optional_args("void*", "remote_buffer"),
    optional_args("tag_t", "tag"),
    optional_args("rcomp_t", "remote_comp"),
    optional_args("void*", "ctx"),
    positional_error_args,
]),
# ##############################################################################
# # End of the definition
# ##############################################################################
]

def get_input():
    return input