// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci.hpp"
#include "lct.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
const char* get_mode(int argc, char** argv)
{
  if (argc >= 2) return argv[1];
  return "pmi";
}

int parse_env(const char* name)
{
  const char* value = std::getenv(name);
  assert(value != nullptr);
  char* end = nullptr;
  long parsed = std::strtol(value, &end, 10);
  assert(end != value && *end == '\0');
  return static_cast<int>(parsed);
}

bool endpoint_readiness_message_received = false;

void endpoint_readiness_handler(lci::status_t status)
{
  assert(status.rank == 0);
  assert(status.size == sizeof(char));
  assert(static_cast<char*>(status.buffer)[0] == 'r');
  std::free(status.buffer);
  endpoint_readiness_message_received = true;
}

void run_pmi_exchange_test()
{
  const int env_rank = parse_env("RANK");
  const int env_size = parse_env("WORLD_SIZE");

  LCT_init();
  LCT_pmi_initialize();
  assert(LCT_pmi_initialized());
  assert(LCT_pmi_get_rank() == env_rank);
  assert(LCT_pmi_get_size() == env_size);

  char key[LCT_PMI_STRING_LIMIT];
  char value[LCT_PMI_STRING_LIMIT];
  std::snprintf(key, sizeof(key), "tcp-test-rank-%d", env_rank);
  std::snprintf(value, sizeof(value), "value-from-%d", env_rank);
  LCT_pmi_publish(key, value);
  LCT_pmi_barrier();

  for (int rank = 0; rank < env_size; ++rank) {
    char query_key[LCT_PMI_STRING_LIMIT];
    char query_value[LCT_PMI_STRING_LIMIT];
    char expected[LCT_PMI_STRING_LIMIT];
    std::snprintf(query_key, sizeof(query_key), "tcp-test-rank-%d", rank);
    std::memset(query_value, 0, sizeof(query_value));
    LCT_pmi_getname(rank, query_key, query_value);
    std::snprintf(expected, sizeof(expected), "value-from-%d", rank);
    assert(std::strcmp(query_value, expected) == 0);
  }

  LCT_pmi_barrier();
  LCT_pmi_finalize();
  assert(!LCT_pmi_initialized());
  LCT_fina();
}

void run_lci_runtime_test()
{
  const int env_rank = parse_env("RANK");
  const int env_size = parse_env("WORLD_SIZE");

  lci::g_runtime_init();
  assert(lci::get_rank_me() == env_rank);
  assert(lci::get_rank_n() == env_size);
  lci::g_runtime_fina();
}

void run_endpoint_readiness_test()
{
  const int env_rank = parse_env("RANK");
  const int env_size = parse_env("WORLD_SIZE");
  assert(env_size == 2);

  lci::g_runtime_init();
  assert(lci::get_rank_me() == env_rank);
  assert(lci::get_rank_n() == env_size);
  std::puts("LCI_ENDPOINT_READINESS_INIT_COMPLETE");
  std::fflush(stdout);

  lci::packet_pool_t packet_pool = lci::alloc_packet_pool();
  lci::device_t device = lci::alloc_device_x().packet_pool(packet_pool)();
  lci::comp_t handler = lci::alloc_handler(endpoint_readiness_handler);
  lci::rcomp_t rcomp = lci::register_rcomp(handler);
  LCT_pmi_barrier();

  if (env_rank == 0) {
    char payload = 'r';
    lci::comp_t sync = lci::alloc_sync();
    lci::status_t status;
    do {
      status = lci::post_am_x(1, &payload, sizeof(payload), sync, rcomp)
                   .device(device)();
      lci::progress_x().device(device)();
    } while (status.is_retry());
    if (status.is_posted()) {
      while (!lci::sync_test(sync, &status)) {
        lci::progress_x().device(device)();
      }
    }
    assert(status.is_done());
    lci::free_comp(&sync);
  } else {
    while (!endpoint_readiness_message_received) {
      lci::progress_x().device(device)();
    }
  }

  lci::wait_drained_x().device(device)();
  LCT_pmi_barrier();
  lci::deregister_rcomp(rcomp);
  lci::free_comp(&handler);
  lci::free_device(&device);
  lci::free_packet_pool(&packet_pool);
  lci::g_runtime_fina();
}

void run_autodetect_fallback_test()
{
  // RANK/WORLD_SIZE alone must not make the default backend chain select
  // tcp and then abort due to a missing TCP endpoint. With no endpoint,
  // the chain should fall through to the always-available local backend.
  LCT_init();
  LCT_pmi_initialize();
  assert(LCT_pmi_initialized());
  assert(LCT_pmi_get_rank() == 0);
  assert(LCT_pmi_get_size() == 1);
  LCT_pmi_finalize();
  assert(!LCT_pmi_initialized());
  LCT_fina();
}
}  // namespace

int main(int argc, char** argv)
{
  const char* mode = get_mode(argc, argv);
  if (std::strcmp(mode, "pmi") == 0) {
    run_pmi_exchange_test();
  } else if (std::strcmp(mode, "runtime") == 0) {
    run_lci_runtime_test();
  } else if (std::strcmp(mode, "endpoint-readiness") == 0) {
    run_endpoint_readiness_test();
  } else if (std::strcmp(mode, "fallback-local") == 0) {
    run_autodetect_fallback_test();
  } else {
    std::fprintf(stderr, "Unknown test mode '%s'\n", mode);
    return 2;
  }
  return 0;
}
