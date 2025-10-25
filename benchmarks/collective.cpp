// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <iostream>
#include <thread>
#include <cassert>
#include <chrono>
#include <atomic>
#include <string>
#include <cxxopts.hpp>
#include "lct.h"
#include "lci.hpp"

struct config_t {
  bool blocking = false;
  size_t nsteps = 10000;
} g_config;

int main(int argc, char** argv)
{
  cxxopts::Options options("lci_collective", "Barrier performance benchmark");
  options.add_options()
      ("b,blocking", "Use blocking barrier", cxxopts::value<bool>()->default_value(g_config.blocking ? "true" : "false")->implicit_value("true"))
      ("n,nsteps", "Number of barrier iterations", cxxopts::value<size_t>()->default_value(std::to_string(g_config.nsteps)))
      ("h,help", "Print help")
      ;
  auto result = options.parse(argc, argv);
  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  g_config.blocking = result["blocking"].as<bool>();
  g_config.nsteps = result["nsteps"].as<size_t>();
  if (g_config.nsteps == 0) {
    std::cerr << "nsteps must be greater than zero." << std::endl;
    return 1;
  }

  lci::g_runtime_init();

  lci::barrier();
  auto start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < g_config.nsteps; ++i) {
    if (g_config.blocking) {
      lci::barrier();
    } else {
      lci::comp_t comp = lci::alloc_sync();
      lci::barrier_x().comp(comp)();
      while (!lci::sync_test(comp, nullptr)) {
        lci::progress();
      }
      lci::free_comp(&comp);
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  if (lci::get_rank_me() == 0) {
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Blocking: " << (g_config.blocking ? "true" : "false") << std::endl;
    std::cout << "Number of steps: " << g_config.nsteps << std::endl;
    std::cout << "Elapsed time: " << elapsed.count() << " s\n";
    std::cout << "Time per barrier: " << elapsed.count() * 1e6 / g_config.nsteps << " us\n";
  }

  lci::g_runtime_fina();
  return 0;
}
