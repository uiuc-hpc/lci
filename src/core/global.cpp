#include "lcixx_internal.hpp"
#include <unistd.h>

namespace lcixx
{
int g_rank = -1, g_nranks = -1;
runtime_t g_default_runtime;

void init_global_attr();
namespace
{
std::atomic<int> global_ini_counter(0);

void global_config_initialize()
{
  init_global_attr();
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
    g_default_attr.backend = option_backend_t::ofi;
  } else if (default_backend == "ibv" || default_backend == "IBV") {
    g_default_attr.backend = option_backend_t::ibv;
  } else if (default_backend == "ucx" || default_backend == "UCX") {
    g_default_attr.backend = option_backend_t::ucx;
  } else {
    LCIXX_Assert(false, "Unknown backend: %s\n", default_backend.c_str());
  }
  {
    // default value
    g_default_attr.net_lock_mode = 0;
    // if users explicitly set the value
    char* p = getenv("LCIXX_NET_LOCK_MODE");
    if (p) {
      LCT_dict_str_int_t dict[] = {
          {"none", 0},
          {"send", LCIXX_NET_TRYLOCK_SEND},
          {"recv", LCIXX_NET_TRYLOCK_RECV},
          {"poll", LCIXX_NET_TRYLOCK_POLL},
      };
      g_default_attr.net_lock_mode =
          LCT_parse_arg(dict, sizeof(dict) / sizeof(dict[0]), p, ",");
    }
    LCIXX_Assert(g_default_attr.net_lock_mode < LCIXX_NET_TRYLOCK_MAX,
                 "Unexpected LCIXX_NET_LOCK_MODE %d",
                 g_default_attr.net_lock_mode);
    LCIXX_Log(LOG_INFO, "env", "set LCIXX_NET_LOCK_MODE to be %d\n",
              g_default_attr.net_lock_mode);
  }
}
}  // namespace

void global_initialize()
{
  if (global_ini_counter++ > 0) return;

  if (getenv("LCIXX_INIT_ATTACH_DEBUGGER")) {
    int i = 1;
    printf("PID %d is waiting to be attached\n", getpid());
    while (i) continue;
  }
  LCT_init();
  log_initialize();
  // Initialize PMI.
  LCT_pmi_initialize();
  g_rank = LCT_pmi_get_rank();
  g_nranks = LCT_pmi_get_size();
  LCIXX_Assert(g_nranks > 0, "PMI ran into an error (num_proc=%d)\n", g_nranks);
  LCT_set_rank(g_rank);
  // Initialize global configuration.
  global_config_initialize();
  pcounter_init();
}

void global_finalize()
{
  if (--global_ini_counter > 0) return;

  pcounter_fina();
  LCT_pmi_finalize();
  log_finalize();
  LCT_fina();
}

int get_rank_x::call_impl() const { return g_rank; }

int get_nranks_x::call_impl() const { return g_nranks; }

}  // namespace lcixx