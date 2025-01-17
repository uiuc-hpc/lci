import sys, os
sys.path.append(os.path.dirname(__file__))
from tools import *

input = [
# obj runtime:
#   bool use_reg_cache
#   bool use_control_channel
#   bool use_default_net_context
#   bool use_default_net_device
#   option_rdv_protocol_t rdv_protocol
resource_runtime := resource("runtime", [
    attr("bool", "use_reg_cache"),
    attr("bool", "use_control_channel"),
    attr("bool", "use_default_net_context"),
    attr("bool", "use_default_net_device"),
    attr("option_rdv_protocol_t", "rdv_protocol")
]),
operation_alloc(resource_runtime, add_runtime_args=False),
operation_free(resource_runtime, add_runtime_args=False),
operation("g_runtime_init", [
    error_args,
    optional_args("bool", "use_reg_cache"),
    optional_args("bool", "use_control_channel"),
    optional_args("bool", "use_default_net_context"),
    optional_args("bool", "use_default_net_device"),
    optional_args("option_rdv_protocol_t", "rdv_protocol")
]),
operation("g_runtime_fina", [
    error_args,
]),
#   op get_rank:
#     COMMON_ARGS
#     int* rank
operation("get_rank", [
    runtime_args,
    error_args,
    positional_args("int*", "rank")
]),
# op get_nranks;
#   optional runtime_t runtime
#   optional error_t* error
#   int* nranks
operation("get_nranks", [
    runtime_args,
    error_args,
    positional_args("int*", "nranks")
]),
operation("get_default_net_context", [
    runtime_args,
    error_args,
    positional_args("net_context_t*", "net_context")
]),
operation("get_default_net_device", [
    runtime_args,
    error_args,
    positional_args("net_device_t*", "net_device")
]),
# obj net_context:
#   option_backend_t backend
#   std::string provider_name
#   int64_t max_msg_size
resource_net_context := resource("net_context", [
    attr("option_backend_t", "backend"),
    attr("std::string", "provider_name"),
    attr("int64_t", "max_msg_size")
]),
operation_alloc(resource_net_context),
operation_free(resource_net_context),
# obj net_device:
#   int64_t max_sends
#   int64_t max_recvs
#   int64_t max_cqes
resource_net_device := resource("net_device", [
    attr("int64_t", "max_sends"),
    attr("int64_t", "max_recvs"),
    attr("int64_t", "max_cqes")
]),
operation_alloc(resource_net_device, [
    optional_args("net_context_t", "net_context")
]),
operation_free(resource_net_device),
# obj mr:
resource("mr", [
]),
# op register_memory:
#   optional runtime_t runtime
#   optional error_t* error
#   net_device_t device
#   void* address
#   size_t size
#   mr_t* mr
operation("register_memory", [
    runtime_args,
    error_args,
    positional_args("net_device_t", "device"),
    positional_args("void*", "address"),
    positional_args("size_t", "size"),
    positional_args("mr_t*", "mr")
]),
# op deregister_memory:
#   optional runtime_t runtime
#   optional error_t* error
#   mr_t mr
operation("deregister_memory", [
    runtime_args,
    error_args,
    positional_args("mr_t", "mr")
]),
# op get_rkey:
#   optional runtime_t runtime
#   optional error_t* error
#   rkey_t* rkey
operation("get_rkey", [
    runtime_args,
    error_args,
    positional_args("rkey_t*", "rkey")
]),
# op net_poll_cq:
#   optional runtime_t runtime
#   optional error_t* error
#   net_device_t device
#   optional int max_polls
#   std::vector<net_status_t>* statuses
operation("net_poll_cq", [
    runtime_args,
    error_args,
    positional_args("net_device_t", "device"),
    optional_args("int", "max_polls"),
    positional_args("std::vector<net_status_t>*", "statuses")
]),
# obj net_endpoint:
resource("net_endpoint", [
]),
# op alloc_net_endpoint:
#   optional runtime_t runtime
#   optional error_t* error
#   net_endpoint_t* net_endpoint
operation("alloc_net_endpoint", [
    runtime_args,
    error_args,
    positional_args("net_endpoint_t*", "net_endpoint")
]),
# op free_net_endpoint:
#   optional runtime_t runtime
#   optional error_t* error
#   net_endpoint_t net_endpoint
operation("free_net_endpoint", [
    runtime_args,
    error_args,
    positional_args("net_endpoint_t", "net_endpoint")
]),
]

def get_input():
    return input