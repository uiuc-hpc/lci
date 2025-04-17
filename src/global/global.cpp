// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"
#include <unistd.h>

namespace lci
{
bool g_is_active = false;
int g_rank_me = -1, g_rank_n = -1;
// TODO: make the default runtime a thread_local stack
// and let users to switch runtime via function calls
// instead of optional arguments
runtime_t g_default_runtime;

namespace internal_config
{
bool enable_bootstrap_lci = true;
}  // namespace internal_config

void init_global_attr();
namespace
{
std::atomic<int> global_ini_counter(0);

void global_config_initialize()
{
  init_global_attr();
  // backend
  std::string default_backend;
  const char* env = getenv("LCI_NETWORK_BACKEND_DEFAULT");
  if (env) {
    default_backend = env;
  } else {
    // parse LCI_NETWORK_BACKENDS_ENABLED and use the first entry as the
    // default
    std::string s(LCI_NETWORK_BACKENDS_ENABLED);
    std::string::size_type pos = s.find(';');
    if (pos != std::string::npos) {
      default_backend = s.substr(0, pos);
    } else {
      default_backend = s;
    }
  }
  if (default_backend == "ofi" || default_backend == "OFI") {
    g_default_attr.backend = attr_backend_t::ofi;
  } else if (default_backend == "ibv" || default_backend == "IBV") {
    g_default_attr.backend = attr_backend_t::ibv;
  } else if (default_backend == "ucx" || default_backend == "UCX") {
    g_default_attr.backend = attr_backend_t::ucx;
  } else {
    LCI_Assert(false, "Unknown backend: %s\n", default_backend.c_str());
  }
  {
    // default value
    g_default_attr.ofi_lock_mode = 0;
    // if users explicitly set the value
    char* p = getenv("LCI_NET_LOCK_MODE");
    if (p) {
      LCT_dict_str_int_t dict[] = {
          {"none", 0},
          {"send", LCI_NET_TRYLOCK_SEND},
          {"recv", LCI_NET_TRYLOCK_RECV},
          {"poll", LCI_NET_TRYLOCK_POLL},
      };
      g_default_attr.ofi_lock_mode =
          LCT_parse_arg(dict, sizeof(dict) / sizeof(dict[0]), p, ",");
    }
    LCI_Assert(g_default_attr.ofi_lock_mode < LCI_NET_TRYLOCK_MAX,
               "Unexpected LCI_NET_LOCK_MODE %d", g_default_attr.ofi_lock_mode);
    LCI_Log(LOG_INFO, "env", "set LCI_NET_LOCK_MODE to be %d\n",
            g_default_attr.ofi_lock_mode);
  }
  internal_config::enable_bootstrap_lci = get_env_or(
      "LCI_ENABLE_BOOTSTRAP_LCI", internal_config::enable_bootstrap_lci);
}
}  // namespace

void global_initialize()
{
  if (global_ini_counter++ > 0) return;

  if (getenv("LCI_INIT_ATTACH_DEBUGGER")) {
    volatile int i = 1;
    printf("PID %d is waiting to be attached\n", getpid());
    while (i) continue;
  }
  LCT_init();
  log_initialize();
  // Initialize PMI.
  bootstrap::initialize();
  g_rank_me = bootstrap::get_rank_me();
  g_rank_n = bootstrap::get_rank_n();
  LCI_Assert(g_rank_n > 0, "PMI ran into an error (num_proc=%d)\n", g_rank_n);
  LCT_set_rank(g_rank_me);
  // Initialize global configuration.
  global_config_initialize();
  pcounter_init();
  g_is_active = true;
}

void global_finalize()
{
  if (--global_ini_counter > 0) return;

  g_is_active = false;
  pcounter_fina();
  bootstrap::finalize();
  log_finalize();
  LCT_fina();
  g_rank_me = -1;
  g_rank_n = -1;
}

int get_rank_me_x::call_impl() const { return g_rank_me; }

int get_rank_n_x::call_impl() const { return g_rank_n; }

}  // namespace lci