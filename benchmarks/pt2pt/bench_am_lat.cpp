// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#include <omp.h>
#include <cxxopts.hpp>

#include "lci.hpp"

struct config_t {
  int nthreads = 1;
  int ndevices = -1;
  size_t msg_size = 8;
  size_t niters = 1000;
} g_config;

static void wait_for_reply(lci::device_t device, lci::comp_t cq, int thread_id)
{
  lci::status_t status;
  do {
    lci::progress_x().device(device)();
    status = lci::cq_pop(cq);
  } while (status.is_retry());
  assert(status.tag == thread_id);
  assert(status.size == g_config.msg_size);
  // Verify message content
  for (size_t j = 0; j < g_config.msg_size; j++) {
    assert(((char*)status.buffer)[j] == status.rank);
  }
  free(status.buffer);
}

static void worker(int peer_rank, lci::device_t device, lci::rcomp_t rcomp)
{
  int thread_id = omp_get_thread_num();
  int nthreads = omp_get_num_threads();
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  bool is_initiator = nranks == 1 || rank < nranks / 2;

  lci::comp_t cq = lci::alloc_cq();
  lci::register_rcomp_x(cq).rcomp(rcomp)();
  // make sure the peer has registered the rcomp
  if (thread_id == 0) {
    lci::barrier_x().device(device)();
    lci::wait_drained_x().device(device)();
  }
  #pragma omp barrier
  if (thread_id == 0) {
    lci::barrier_x().device(device)();
  }

  std::vector<char> send_buf(g_config.msg_size, static_cast<char>(lci::get_rank_me()));

  // We need a wait_quiet before every external blocking call
  lci::wait_drained_x().device(device)();
  #pragma omp barrier
  auto start = std::chrono::high_resolution_clock::now();
  for (size_t iter = 0; iter < g_config.niters; ++iter) {
    if (is_initiator) {
      lci::status_t status;
      do {
        status = lci::post_am_x(peer_rank,
                                send_buf.data(),
                                g_config.msg_size,
                                lci::COMP_NULL_RETRY,
                                rcomp)
                     .device(device)
                     .tag(thread_id)();
        lci::progress_x().device(device)();
      } while (status.is_retry());
      wait_for_reply(device, cq, thread_id);
    } else {
      wait_for_reply(device, cq, thread_id);
      lci::status_t status;
      do {
        status = lci::post_am_x(peer_rank,
                                send_buf.data(),
                                g_config.msg_size,
                                lci::COMP_NULL_RETRY,
                                rcomp)
                     .device(device)
                     .tag(thread_id)();
        lci::progress_x().device(device)();
      } while (status.is_retry());
    }
  }
  // We need a wait_quiet before every external blocking call
  lci::wait_drained_x().device(device)();
  #pragma omp barrier
  auto end = std::chrono::high_resolution_clock::now();

  lci::free_comp(&cq);

  if (rank == 0 && thread_id == 0) {
    std::chrono::duration<double> elapsed = end - start;
    double total_time = elapsed.count();
    if (g_config.niters > 0 && total_time > 0.0) {
      double round_trip_latency = total_time / static_cast<double>(g_config.niters);
      double message_rate = (static_cast<double>(g_config.niters) * nthreads * nranks) / total_time;
      std::cout << "Total time: " << total_time << " s\n";
      std::cout << "Average round-trip latency: " << round_trip_latency * 1e6 << " us\n";
      std::cout << "Bi-directional message rate: " << message_rate / 1e6 << " Mmsgs/s\n";
    } else {
      std::cout << "Insufficient iterations or time to compute metrics.\n";
    }
  }
}

int main(int argc, char** argv)
{
  cxxopts::Options options("lci_bench_am_latency", "Active message pingpong latency test");
  options.add_options()
      ("t,nthreads", "Number of threads", cxxopts::value<int>()->default_value(std::to_string(g_config.nthreads)))
      ("d,ndevices", "Number of devices", cxxopts::value<int>()->default_value(std::to_string(g_config.ndevices)))
      ("s,msg-size", "Message size (bytes)", cxxopts::value<size_t>()->default_value(std::to_string(g_config.msg_size)))
      ("n,niters", "Number of iterations", cxxopts::value<size_t>()->default_value(std::to_string(g_config.niters)))
      ("h,help", "Print help");

  auto result = options.parse(argc, argv);
  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  g_config.nthreads = result["nthreads"].as<int>();
  g_config.ndevices = result["ndevices"].as<int>();
  g_config.msg_size = result["msg-size"].as<size_t>();
  g_config.niters = result["niters"].as<size_t>();

  if (g_config.ndevices == -1) {
    g_config.ndevices = g_config.nthreads;
  }
  
  // Adjust the packet number based on the number of devices
  lci::global_initialize();
  auto attr = lci::get_g_default_attr();
  attr.npackets = attr.npackets * g_config.ndevices;
  lci::set_g_default_attr(attr);

  lci::g_runtime_init_x().alloc_default_device(false)();

  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  assert(nranks == 1 || nranks % 2 == 0);
  int peer_rank;
  if (nranks == 1) {
    peer_rank = rank;
  } else {
    peer_rank = (rank + nranks / 2) % nranks;
  }

  if (rank == 0) {
    std::cout << "Running with " << g_config.nthreads << " threads, "
              << g_config.ndevices << " devices, "
              << g_config.msg_size << " bytes, "
              << g_config.niters << " iterations" << std::endl;
  }

  std::vector<lci::device_t> devices(g_config.ndevices);
  for (int i = 0; i < g_config.ndevices; ++i) {
    devices[i] = lci::alloc_device();
  }
  lci::rcomp_t rcomps = lci::reserve_rcomps(g_config.nthreads);

#pragma omp parallel num_threads(g_config.nthreads)
  {
    int thread_id = omp_get_thread_num();
    int device_idx = thread_id % g_config.ndevices;
    worker(peer_rank, devices[device_idx], rcomps + thread_id);
    lci::wait_drained_x().device(devices[device_idx])();
  }

  for (auto& dev : devices) {
    lci::free_device(&dev);
  }

  lci::g_runtime_fina();
  lci::global_finalize();
  return 0;
}
