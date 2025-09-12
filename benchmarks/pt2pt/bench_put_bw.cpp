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
  int ndevices = -1;
  size_t nelems = 65536;
  size_t niters = 10;
} g_config;

void worker(int peer_rank, lci::device_t device, int *data, lci::rmr_t rmr, lci::comp_t comp, lci::rcomp_t rcomp)
{
  int thread_id = omp_get_thread_num();
  int nthreads = omp_get_num_threads();
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();

  for (size_t i = 0; i < g_config.niters; i++) {
    for (size_t j = thread_id; j < g_config.nelems; j += nthreads) {
      lci::status_t status;
      do {
        status = lci::post_put_x(peer_rank, data + j, sizeof(int), comp, j * sizeof(int), rmr).device(device).allow_done(false)();
        lci::progress_x().device(device)();
      } while (status.is_retry());
    }
  }
  size_t expected = g_config.niters * g_config.nelems;
  while (expected > lci::counter_get(comp)) {
    lci::progress_x().device(device)();
  }
  // One am to signal the end of the test
  lci::status_t status;
  do {
    status = lci::post_am_x(peer_rank, nullptr, 0, lci::COMP_NULL_RETRY, rcomp).device(device).comp_semantic(lci::comp_semantic_t::network)();
    lci::progress_x().device(device)();
  } while (status.is_retry());
}

int main(int argc, char** argv)
{
  cxxopts::Options options("lci_bench_put_bw", "Bandwidth test");
  options.add_options()
      ("t,nthreads", "Number of threads", cxxopts::value<int>()->default_value(std::to_string(g_config.nthreads)))
      ("d,ndevices", "Number of devices", cxxopts::value<int>()->default_value(std::to_string(g_config.ndevices)))
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
  g_config.ndevices = result["ndevices"].as<int>();
  g_config.nelems = result["nelems"].as<size_t>();
  g_config.niters = result["niters"].as<size_t>();

  if (g_config.ndevices == -1) {
    g_config.ndevices = g_config.nthreads;
  }
  
  // Adjust the packet number based on the number of devices
  lci::global_initialize();
  auto attr = lci::get_g_default_attr();
  attr.npackets = attr.npackets * g_config.ndevices;
  lci::set_g_default_attr(attr);

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
  bool is_receiver = nranks == 1 || rank >= nranks / 2;

  if (rank == 0) {
    std::cout << "Running with " << g_config.nthreads << " threads, "
              << g_config.ndevices << " devices, "
              << g_config.nelems << " elements, "
              << g_config.niters << " iterations" << std::endl;
  }

  // allocate devices
  std::vector<lci::device_t> devices;
  devices.resize(g_config.ndevices);
  for (int i = 0; i < g_config.ndevices; i++) {
      devices[i] = lci::alloc_device();
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
  std::vector<lci::mr_t> mrs;
  std::vector<lci::rmr_t> rmrs;
  std::vector<lci::rmr_t> all_rmrs;
  for (int i = 0; i < g_config.ndevices; i++) {
    lci::mr_t mr = lci::register_memory_x(data, size).device(devices[i])();
    mrs.push_back(mr);
    lci::rmr_t rmr = lci::get_rmr(mr);
    rmrs.push_back(rmr);
  }
  all_rmrs.resize(nranks * g_config.ndevices);
  lci::allgather(rmrs.data(), all_rmrs.data(), sizeof(lci::rmr_t) * g_config.ndevices);
  std::vector<lci::rmr_t> peer_rmrs(all_rmrs.begin() + peer_rank * g_config.ndevices,
                                    all_rmrs.begin() + (peer_rank + 1) * g_config.ndevices);
  // allocate completion counter
  lci::comp_t comp = lci::alloc_counter();
  lci::rcomp_t rcomp = lci::register_rcomp(comp);

  lci::barrier_x().comp_semantic(lci::comp_semantic_t::network)();
  auto start = std::chrono::high_resolution_clock::now();
  if (is_sender) {
    #pragma omp parallel num_threads(g_config.nthreads)
    {
      int device_idx = omp_get_thread_num() % g_config.ndevices;
      worker(peer_rank, devices[device_idx], (int*)data, peer_rmrs[device_idx], comp, rcomp);
    }
  }
  if (is_receiver) {
    #pragma omp parallel num_threads(g_config.nthreads)
    {
      int device_idx = omp_get_thread_num() % g_config.ndevices;
      while (lci::counter_get(comp) < g_config.nthreads) {
        lci::progress_x().device(devices[device_idx])();
      }
    }
  }
  lci::barrier_x().comp_semantic(lci::comp_semantic_t::network)();
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  double total_msgs = g_config.niters * g_config.nelems * (nranks == 1 ? 1 : nranks / 2);
  if (rank == 0)
    std::cout << "Elapsed time: " << elapsed.count() << " seconds; "
              << "Message rate: " << (total_msgs / elapsed.count() / 1e6) << " Mmsgs/sec" << std::endl;
  // verify data
  if (!is_sender) {
    for (size_t i = 0; i < g_config.nelems; ++i) {
      assert(((int*)data)[i] == i + 1);
    }
  }

  // cleanup
  lci::free_comp(&comp);
  for (auto& mr : mrs) {
    lci::deregister_memory(&mr);
  }
  free(data);
  for (auto& dev : devices) {
    lci::free_device(&dev);
  }
  lci::g_runtime_fina();
  lci::global_finalize();
  return 0;
}
