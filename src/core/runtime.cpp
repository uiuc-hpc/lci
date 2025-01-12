#include "lcixx_internal.hpp"
#include <unistd.h>

namespace lcixx
{
/*************************************************************
 * Global Initialization and Finalization
 *************************************************************/
void global_initialize()
{
  if (getenv("LCIXX_INIT_ATTACH_DEBUGGER")) {
    int i = 1;
    printf("PID %d is waiting to be attached\n", getpid());
    while (i) continue;
  }
  LCT_init();
  log_initialize();
  // Initialize PMI.
  LCT_pmi_initialize();
  int rank = LCT_pmi_get_rank();
  int num_proc = LCT_pmi_get_size();
  LCIXX_Assert(num_proc > 0, "PMI ran into an error (num_proc=%d)\n", num_proc);
  LCT_set_rank(rank);
}

void global_finalize()
{
  LCT_pmi_finalize();
  log_finalize();
  LCT_fina();
}

/*************************************************************
 * runtime: User wrappers
 *************************************************************/
int runtime_t::get_rank() const { return p_impl->get_rank(); }

int runtime_t::get_nranks() const { return p_impl->get_nranks(); }

runtime_t::config_t runtime_t::get_config() const { return p_impl->config; }

net_context_t runtime_t::get_default_net_context() const
{
  return p_impl->net_context;
}

net_device_t runtime_t::get_default_net_device() const
{
  return p_impl->net_device;
}

runtime_t alloc_runtime_x::call()
{
  runtime_t runtime;
  runtime_t::config_t config;
  runtime.p_impl = new runtime_impl_t(config);
  runtime.p_impl->initialize(runtime);
  return runtime;
}

void free_runtime_x::call() { delete runtime.p_impl; }

/*************************************************************
 * runtime implementation
 *************************************************************/
int runtime_impl_t::g_nruntimes = 0;

runtime_impl_t::runtime_impl_t(config_t config_) : config(config_)
{
  if (g_nruntimes++ == 0) {
    global_initialize();
  }
  rank = LCT_pmi_get_rank();
  nranks = LCT_pmi_get_size();
}

runtime_impl_t::~runtime_impl_t()
{
  if (--g_nruntimes == 0) {
    global_finalize();
  }
}

void runtime_impl_t::initialize(runtime_t runtime_)
{
  runtime = runtime_;

  if (config.backend != option_backend_t::none) {
    net_context =
        alloc_net_context_x(runtime).set_backend(config.backend).call();
  }

  if (net_context.get_config().backend == option_backend_t::ofi &&
      net_context.get_config().provider_name == "cxi") {
    // special setting for libfabric/cxi
    LCIXX_Assert(config.use_reg_cache == false,
                 "The registration cache should be turned off "
                 "for libfabric cxi backend. Use `export LCIXX_USE_DREG=0`.\n");
    LCIXX_Assert(config.use_control_channel == 0,
                 "The progress-specific network endpoint "
                 "for libfabric cxi backend. Use `export "
                 "LCIXX_ENABLE_PRG_NET_ENDPOINT=0`.\n");
    if (config.rdv_protocol != option_rdv_protocol_t::write) {
      config.rdv_protocol = option_rdv_protocol_t::write;
      LCIXX_Warn(
          "Switch LCIXX_RDV_PROTOCOL to \"write\" "
          "as required by the libfabric cxi backend\n");
    }
  }
}

}  // namespace lcixx