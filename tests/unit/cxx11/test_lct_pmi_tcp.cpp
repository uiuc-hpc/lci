// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci.hpp"
#include "lct.h"
#include "bootstrap/bootstrap.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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

// Mimics a caller (e.g. MPI rank 0) that binds the rendezvous socket to an
// OS-chosen port and hands it to LCT through LCT_MASTER_FD. With
// matching_port, the bound port is exported as LCT_MASTER_PORT and written to
// LCT_TEST_PORT_FILE for the other ranks. Without it, LCT_MASTER_PORT is set
// to a different port, so LCT must reject the socket.
int open_caller_listener(bool matching_port)
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(fd >= 0);
  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  int ret = bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
  assert(ret == 0);
  ret = listen(fd, 1);
  assert(ret == 0);
  socklen_t len = sizeof(addr);
  ret = getsockname(fd, reinterpret_cast<struct sockaddr*>(&addr), &len);
  assert(ret == 0);
  (void)ret;
  const int bound_port = ntohs(addr.sin_port);
  const std::string port =
      std::to_string(matching_port ? bound_port : bound_port % 65535 + 1);
  setenv("LCT_MASTER_PORT", port.c_str(), 1);
  setenv("LCT_MASTER_FD", std::to_string(fd).c_str(), 1);

  if (matching_port) {
    const char* port_file = std::getenv("LCT_TEST_PORT_FILE");
    assert(port_file != nullptr);
    const std::string tmp = std::string(port_file) + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    assert(f != nullptr);
    std::fprintf(f, "%s\n", port.c_str());
    std::fclose(f);
    // Rename so that the other ranks never read a partially written file.
    if (std::rename(tmp.c_str(), port_file) != 0) std::abort();
  }
  return fd;
}

void run_master_fd_test(bool matching_port)
{
  int fd = -1;
  if (parse_env("RANK") == 0) fd = open_caller_listener(matching_port);
  run_pmi_exchange_test();
  if (fd >= 0) {
    // LCT borrows the caller's socket and must not close it.
    assert(fcntl(fd, F_GETFD) != -1);
    close(fd);
  }
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

void run_bootstrap_allgather_test()
{
  constexpr size_t buffer_size = LCT_PMI_STRING_LIMIT + 1;

  LCT_init();
  lci::bootstrap::initialize();

  const int rank = lci::bootstrap::get_rank_me();
  const int size = lci::bootstrap::get_rank_n();
  std::vector<uint8_t> sendbuf(buffer_size);
  std::vector<uint8_t> recvbuf(size * buffer_size);
  for (size_t i = 0; i < buffer_size; i++)
    sendbuf[i] = static_cast<uint8_t>(rank + i);

  lci::bootstrap::allgather(sendbuf.data(), recvbuf.data(), buffer_size);

  for (int peer = 0; peer < size; peer++) {
    for (size_t i = 0; i < buffer_size; i++) {
      assert(recvbuf[peer * buffer_size + i] == static_cast<uint8_t>(peer + i));
    }
  }

  lci::bootstrap::finalize();
  LCT_fina();
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
  } else if (std::strcmp(mode, "bootstrap") == 0) {
    run_bootstrap_allgather_test();
  } else if (std::strcmp(mode, "master-fd") == 0) {
    run_master_fd_test(true);
  } else if (std::strcmp(mode, "master-fd-mismatch") == 0) {
    run_master_fd_test(false);
  } else if (std::strcmp(mode, "fallback-local") == 0) {
    run_autodetect_fallback_test();
  } else {
    std::fprintf(stderr, "Unknown test mode '%s'\n", mode);
    return 2;
  }
  return 0;
}
