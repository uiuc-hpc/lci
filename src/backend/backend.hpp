#ifndef LCIXX_BACKEND_BACKEND_HPP
#define LCIXX_BACKEND_BACKEND_HPP

namespace lcixx {

class net_context_impl_t {
public:
    net_context_impl_t(runtime_t runtime_, net_context_t::config_t config_) : runtime(runtime_), config(config_) {};
    ~net_context_impl_t() = default;

    virtual net_device_t alloc_net_device(net_device_t::config_t config) = 0;

    runtime_t runtime;
    net_context_t::config_t config;
};

class net_device_impl_t {
public:
    static std::atomic<int> g_ndevices;

    using config_t = net_device_t::config_t;

    net_device_impl_t(net_context_t context_, config_t config_) : context(context_), config(config_) {
        net_device_id = g_ndevices++;
        runtime = context.p_impl->runtime;
    };

    virtual mr_t register_memory(void* address, size_t size) = 0;
    virtual void deregister_memory(mr_t) = 0;

    runtime_t runtime;
    net_context_t context;
    config_t config;
    int net_device_id;
};

class mr_impl_t {
public:
    // TODO: add memory registration cache
    // For memory registration cache.
    // net_device_t device;
    // void* region;
};

} // namespace lcixx

#endif // LCIXX_BACKEND_BACKEND_HPP