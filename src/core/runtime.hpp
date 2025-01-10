#ifndef LCIXX_CORE_RUNTIME_HPP
#define LCIXX_CORE_RUNTIME_HPP

#include "lcixx_internal.hpp"

namespace lcixx {
class runtime_impl_t {
public:
    using config_t = runtime_t::config_t;
    runtime_impl_t(config_t);
    ~runtime_impl_t();
    int get_rank() const { return rank; }
    int get_nranks() const { return nranks; }
    void initialize(runtime_t);

    static int g_nruntimes;

    runtime_t runtime;
    runtime_t::config_t config;
    int rank, nranks;
    net_context_t net_context;
};
} // namespace lcixx

#endif // LCIXX_CORE_RUNTIME_HPP