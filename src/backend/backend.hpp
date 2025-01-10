#ifndef LCIXX_BACKEND_BACKEND_HPP
#define LCIXX_BACKEND_BACKEND_HPP

namespace lcixx {

class net_context_impl_t {
public:
    net_context_impl_t(runtime_t runtime_, net_context_t::config_t config_) : runtime(runtime_), config(config_) {};
    ~net_context_impl_t() = default;
    runtime_t runtime;
    net_context_t::config_t config;
};

class net_device_impl_t {
public:
    static std::atomic<int> g_ndevices;

    net_device_impl_t(net_context_t context_, net_device_t::config_t config_) : context(context_), config(config_) {
        net_device_id = g_ndevices++;
        runtime = context.p_impl->runtime;
    };

    runtime_t runtime;
    net_context_t context;
    net_device_t::config_t config;
    int net_device_id;
};

} // namespace lcixx

#endif // LCIXX_BACKEND_BACKEND_HPP