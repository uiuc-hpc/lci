#include "lcixx_internal.hpp"
#include <unistd.h>

namespace lcixx
{
namespace
{
void global_config_initialize()
{
  // backend
  std::string default_backend;
  const char* env = getenv("LCIXX_NETWORK_BACKEND_DEFAULT");
  if (env) {
    default_backend = env;
  } else {
    // parse LCIXX_NETWORK_BACKENDS_ENABLED and use the first entry as the
    // default
    std::string s(LCIXX_NETWORK_BACKENDS_ENABLED);
    std::string::size_type pos = s.find(';');
    if (pos != std::string::npos) {
      default_backend = s.substr(0, pos);
    } else {
      default_backend = s;
    }
  }
  if (default_backend == "ofi" || default_backend == "OFI") {
    g_default_attr.net_context_attr.backend = option_backend_t::ofi;
  } else if (default_backend == "ibv" || default_backend == "IBV") {
    g_default_attr.net_context_attr.backend = option_backend_t::ibv;
  } else if (default_backend == "ucx" || default_backend == "UCX") {
    g_default_attr.net_context_attr.backend = option_backend_t::ucx;
  } else {
    LCIXX_Assert(false, "Unknown backend: %s\n", default_backend.c_str());
  }
}
}  // namespace

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
  // Initialize global configuration.
  global_config_initialize();
}

void global_finalize()
{
  LCT_pmi_finalize();
  log_finalize();
  LCT_fina();
}

}  // namespace lcixx