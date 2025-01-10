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

} // namespace lcixx