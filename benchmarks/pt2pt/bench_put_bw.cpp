// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <iostream>
#include <thread>
#include <cassert>
#include <chrono>
#include <atomic>
#include <omp.h>
#include <cxxopts.hpp>
#include "lci.hpp"

struct config_t {
  int nthreads = 1;
  size_t nelems = 65536;
  size_t niters = 10;
} g_config;

void worker(int peer_rank, int *data, lci::rmr_t rmr, lci::comp_t comp)
{
  int thread_id = omp_get_thread_num();
  int nthreads = omp_get_num_threads();
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();

  if (nranks == 1 || rank < nranks / 2) {
    // sender
    for (size_t i = 0; i < g_config.niters; i++) {
      for (size_t j = thread_id; j < g_config.nelems; j += nthreads) {
        lci::status_t status;
        do {
          status = lci::post_put_x(peer_rank, data + j, sizeof(int), comp, j * sizeof(int), rmr).allow_done(false)();
          lci::progress();
        } while (status.is_retry());
      }
    }
  }
  size_t expected = g_config.niters * g_config.nelems;
  while (expected > lci::counter_get(comp)) {
    lci::progress();
  }
}

int main(int argc, char** argv)
{
  cxxopts::Options options("lci_p_bw", "Bandwidth test");
  options.add_options()
      ("t,nthreads", "Number of threads", cxxopts::value<int>()->default_value(std::to_string(g_config.nthreads)))
      ("N,nelems", "Number of elements", cxxopts::value<size_t>()->default_value(std::to_string(g_config.nelems)))
      ("n,niters", "Number of iterations", cxxopts::value<size_t>()->default_value(std::to_string(g_config.niters)))
      ("h,help", "Print help")
      ;
  auto result = options.parse(argc, argv);

  if (result.count("help")) {
      std::cout << options.help() << std::endl;
      return 0;
  }

  g_config.nthreads = result["nthreads"].as<int>();
  g_config.nelems = result["nelems"].as<size_t>();
  g_config.niters = result["niters"].as<size_t>();
  
  lci::g_runtime_init();
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  assert(nranks == 1 || nranks % 2 == 0);
  int peer_rank;
  if (nranks == 1) {
    peer_rank = rank;
  } else {
    peer_rank = (rank + nranks / 2) % nranks;
  }
  bool is_sender = nranks == 1 || rank < nranks / 2;

  if (rank == 0) {
    std::cout << "Running with " << g_config.nthreads << " threads, "
              << g_config.nelems << " elements, "
              << g_config.niters << " iterations" << std::endl;
  }

  // allocate memory
  size_t size = g_config.nelems * sizeof(int);
  void *data = malloc(size);
  for (size_t i = 0; i < g_config.nelems; ++i) {
    if (is_sender) {
      ((int*)data)[i] = i + 1;
    } else {
      ((int*)data)[i] = 0;
    }
  }
  lci::mr_t mr = lci::register_memory(data, size);
  lci::rmr_t my_rmr = lci::get_rmr(mr);
  std::vector<lci::rmr_t> rmrs;
  rmrs.resize(nranks);
  lci::allgather(&my_rmr, rmrs.data(), sizeof(lci::rmr_t));
  lci::rmr_t peer_rmr = rmrs[peer_rank];
  // allocate completion counter
  lci::comp_t comp = lci::alloc_counter();

  lci::barrier();
  if (is_sender) {
    auto start = std::chrono::high_resolution_clock::now();
    #pragma omp parallel num_threads(g_config.nthreads)
    {
      worker(peer_rank, (int*)data, peer_rmr, comp);
    }
    lci::barrier();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double total_msgs = g_config.niters * g_config.nelems;
    std::cout << "Elapsed time: " << elapsed.count() << " seconds; "
              << "Message rate: " << (total_msgs / elapsed.count() / 1e6) << " Mmsgs/sec" << std::endl;
  } else {
    lci::barrier();
  }
  // verify data
  if (!is_sender) {
    for (size_t i = 0; i < g_config.nelems; ++i) {
      assert(((int*)data)[i] == i + 1);
    }
  }

  // cleanup
  lci::free_comp(&comp);
  lci::deregister_memory(&mr);
  free(data);
  lci::g_runtime_fina();
  return 0;
}
