#include "lcixx_internal.hpp"

namespace lcixx {

net_context_t alloc_net_context_x::call() {
    net_context_t ret;
    net_context_t::config_t config;
    switch(backend.get_value_or(g_default_config.backend)) {
        case option_backend_t::none:
            break;
        case option_backend_t::ofi:
            ret.p_impl = std::make_shared<ofi_net_context_impl_t>(runtime, config);
        default:
            LCIXX_Assert(false, "Unsupported backend\n");
    }
    return ret;
}

void free_net_context_x::call() {
    net_context.p_impl.reset();
}

net_context_t::config_t net_context_t::get_config() const {
    return p_impl->config;
}

std::atomic<int> net_device_impl_t::g_ndevices(0);

net_device_t alloc_net_device_x::call() {
    net_device_t ret;
    net_context_t context = context.get_value_or(runtime.get_default_context());
    net_device_t::config_t config;
    config.max_sends = max_sends.get_value_or(g_default_config.backend_max_sends);
    config.max_recvs = max_recvs.get_value_or(g_default_config.backend_max_recvs);
    config.max_cqes = max_cqes.get_value_or(g_default_config.backend_max_cqes);
    ret.p_impl = std::make_shared<ofi_net_device_impl_t>(context, config);
}

} // namespace lcixx