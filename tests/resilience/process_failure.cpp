// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr auto timeout = std::chrono::seconds(10);

std::string test_directory()
{
  const char* value = std::getenv("LCI_RESILIENCE_TEST_DIR");
  if (value == nullptr || value[0] == '\0') {
    throw std::runtime_error("LCI_RESILIENCE_TEST_DIR is not set");
  }
  return value;
}

void touch(const std::string& path)
{
  std::ofstream file(path);
  if (!file) throw std::runtime_error("Failed to create " + path);
}

void wait_for_file(const std::string& path)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    std::ifstream file(path);
    if (file.good()) return;
    lci::progress();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  throw std::runtime_error("Timed out waiting for " + path);
}
}  // namespace

int main()
{
  try {
    lci::g_runtime_init();
    const int rank = lci::get_rank_me();
    if (lci::get_rank_n() != 3) {
      throw std::runtime_error("The process-failure test requires three ranks");
    }

    const std::string directory = test_directory();
    touch(directory + "/ready-" + std::to_string(rank));
    if (rank == 1) {
      for (;;) {
        lci::progress();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }

    wait_for_file(directory + "/start");
    if (rank == 2) {
      wait_for_file(directory + "/failure-caught");
      for (int i = 0; i < 100; ++i) {
        lci::progress();
      }
      lci::g_runtime_fina();
      touch(directory + "/finished-2");
      return 0;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool caught_peer_failure = false;
    const size_t payload_size =
        std::min<size_t>(lci::get_max_bcopy_size(), 4096);
    std::vector<char> payload(payload_size);
    lci::comp_t completion = lci::alloc_cq();
    bool operation_posted = false;
    while (std::chrono::steady_clock::now() < deadline) {
      try {
        if (!operation_posted) {
          lci::status_t post_status =
              lci::post_send_x(1, payload.data(), payload.size(), 0, completion)
                  .comp_semantic(lci::comp_semantic_t::network)();
          if (post_status.is_retry()) continue;
          if (!post_status.is_posted()) {
            throw std::runtime_error(
                "The failed operation did not use posted semantics");
          }
          operation_posted = true;
        }
        lci::progress();
        if (lci::cq_pop(completion).is_done()) {
          operation_posted = false;
        }
      } catch (const lci::peer_failure_error& error) {
        if (error.failed_rank() != 1) {
          std::cerr << "Expected failed rank 1, got " << error.failed_rank()
                    << '\n';
          return 1;
        }
        caught_peer_failure = true;
        break;
      }
    }
    if (!caught_peer_failure) {
      throw std::runtime_error("Did not observe the killed peer's failure");
    }
    if (!operation_posted) {
      throw std::runtime_error(
          "No posted operation remained when the failure was reported");
    }
    if (lci::cq_pop(completion).is_done()) {
      throw std::runtime_error(
          "The failed operation incorrectly signaled successful completion");
    }
    lci::free_comp(&completion);
    touch(directory + "/failure-caught");

    // The failed completion has been consumed. Surviving ranks must be able to
    // continue progressing and then finalize without waiting for the dead rank.
    for (int i = 0; i < 100; ++i) {
      lci::progress();
    }
    lci::g_runtime_fina();
    touch(directory + "/finished-0");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "process-failure test failed: " << error.what() << '\n';
    return 1;
  }
}
