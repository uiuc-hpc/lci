// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"
#include <unistd.h>
#include <signal.h>

namespace lci
{
const char* DEFAULT_NAME = "unnamed";
bool g_is_active = false;
int g_rank_me = -1, g_rank_n = -1;
allocator_default_t g_allocator_default;
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

bool is_backend_available(const std::string& backend)
{
  bool ret = false;
  if (backend == "ofi" || backend == "OFI") {
#ifdef LCI_BACKEND_ENABLE_OFI
    ret = ofi_net_context_impl_t::check_availability();
#else
    ret = false;
#endif
  } else if (backend == "ibv" || backend == "IBV") {
#ifdef LCI_BACKEND_ENABLE_IBV
    ret = ibv_net_context_impl_t::check_availability();
#else
    ret = false;
#endif
  }
  LCI_Log(LOG_INFO, "global", "backend %s availability: %d\n", backend.c_str(),
          ret);
  return ret;
}

void global_config_initialize()
{
  init_global_attr();
  // backend
  std::string default_backend;
  const char* env = getenv("LCI_ATTR_BACKEND");
  if (env) {
    default_backend = env;
    if (!is_backend_available(default_backend)) {
      LCI_Assert(false,
                 "The specified network backend %s is not available! Please "
                 "check your LCI_ATTR_BACKEND setting and your system "
                 "configuration.\n",
                 default_backend.c_str());
    }
  } else {
    // parse LCI_NETWORK_BACKENDS_ENABLED and use the first available entry as
    // the default
    std::vector<std::string> backends;
    // parse LCI_NETWORK_BACKENDS_ENABLED
    std::string s(LCI_NETWORK_BACKENDS_ENABLED);
    size_t start = 0;
    std::string::size_type pos = s.find(';');
    while (pos != std::string::npos) {
      backends.push_back(s.substr(start, pos - start));
      start = pos + 1;
      pos = s.find(';', start);
    }
    backends.push_back(s.substr(start));
    // Check availability
    for (const auto& backend : backends) {
      if (is_backend_available(backend)) {
        default_backend = backend;
        break;
      }
    }
    if (default_backend.empty()) {
      LCI_Assert(false,
                 "No available network backend found! Please check your "
                 "LCI_NETWORK_BACKENDS setting and your system "
                 "configuration.\n");
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

  char* p = getenv("LCI_INIT_ATTACH_DEBUGGER");
  if (p && *p != '\0' && *p != '0') {
    volatile int i = 1;
    fprintf(stderr, "PID %d is waiting to be attached\n", getpid());
    if (std::string(p) == "sigstop") {
      fprintf(stderr, "kill -CONT to resume\n");
      raise(SIGSTOP);
    } else {
      fprintf(stderr, "set var i to 0 to resume\n");
      while (i) continue;
    }
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
#ifdef LCI_USE_CUDA
  accelerator::initialize();
#endif  // LCI_USE_CUDA
  g_is_active = true;
}

void global_finalize()
{
  if (--global_ini_counter > 0) return;

  g_is_active = false;
#ifdef LCI_USE_CUDA
  accelerator::finalize();
#endif  // LCI_USE_CUDA
  pcounter_fina();
  bootstrap::finalize();
  log_finalize();
  LCT_fina();
  g_rank_me = -1;
  g_rank_n = -1;
}

int get_rank_me() { return g_rank_me; }

int get_rank_n() { return g_rank_n; }

global_attr_t get_g_default_attr() { return g_default_attr; }

void set_g_default_attr(const global_attr_t& attr) { g_default_attr = attr; }

}  // namespace lci