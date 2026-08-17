// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#include <cxxopts.hpp>

#include "lci.hpp"

struct config_t {
  size_t msg_size = sizeof(int);
  size_t niters = 1000;
  size_t warmup_iters = 100;
  bool force_posted_writedata = false;
} g_config;

static void wait_remote_completion(lci::device_t device, lci::comp_t comp)
{
  lci::status_t status;
  do {
    lci::progress_x().device(device)();
    status = lci::cq_pop(comp);
  } while (status.is_retry());
  assert(status.is_done());
}

static void post_put(int peer_rank, void* buffer, lci::mr_t mr,
                     lci::rmr_t peer_rmr, lci::device_t device,
                     lci::rcomp_t rcomp)
{
  lci::status_t status;
  do {
    auto op = lci::post_put_x(peer_rank, buffer, g_config.msg_size,
                              lci::COMP_NULL_RETRY, g_config.msg_size, peer_rmr)
                  .device(device)
                  .mr(mr)
                  .remote_comp(rcomp);
    if (g_config.force_posted_writedata) {
      op = op.comp_semantic(lci::comp_semantic_t::network);
    }
    status = op();
    lci::progress_x().device(device)();
  } while (status.is_retry());
}

static void run_pingpong(size_t niters, bool is_initiator, int peer_rank,
                         void* send_buffer, lci::mr_t mr, lci::rmr_t peer_rmr,
                         lci::device_t device, lci::comp_t remote_comp,
                         lci::rcomp_t rcomp)
{
  for (size_t i = 0; i < niters; ++i) {
    if (is_initiator) {
      post_put(peer_rank, send_buffer, mr, peer_rmr, device, rcomp);
      wait_remote_completion(device, remote_comp);
    } else {
      wait_remote_completion(device, remote_comp);
      post_put(peer_rank, send_buffer, mr, peer_rmr, device, rcomp);
    }
  }
}

int main(int argc, char** argv)
{
  cxxopts::Options options("lci_bench_put_lat",
                           "Remote-completion PUT ping-pong latency test");
  options.add_options()("s,msg-size", "Message size (bytes)",
                        cxxopts::value<size_t>()->default_value(
                            std::to_string(g_config.msg_size)))(
      "n,niters", "Measured iterations",
      cxxopts::value<size_t>()->default_value(std::to_string(g_config.niters)))(
      "w,warmup-iters", "Warmup iterations",
      cxxopts::value<size_t>()->default_value(
          std::to_string(g_config.warmup_iters)))(
      "force-posted-writedata",
      "Force remote-completion PUTs through the registered/posted network path",
      cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
      "h,help", "Print help");
  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  g_config.msg_size = result["msg-size"].as<size_t>();
  g_config.niters = result["niters"].as<size_t>();
  g_config.warmup_iters = result["warmup-iters"].as<size_t>();
  g_config.force_posted_writedata = result["force-posted-writedata"].as<bool>();
  assert(g_config.msg_size > 0);
  assert(g_config.niters > 0);

  lci::global_initialize();
  lci::g_runtime_init_x().alloc_default_device(false)();

  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  assert(nranks == 2);
  int peer_rank = 1 - rank;
  bool is_initiator = rank == 0;

  lci::device_t device = lci::alloc_device();
  std::vector<char> data(2 * g_config.msg_size, 0);
  void* send_buffer = data.data();
  void* recv_buffer = data.data() + g_config.msg_size;
  memset(send_buffer, rank + 1, g_config.msg_size);

  lci::mr_t mr =
      lci::register_memory_x(data.data(), data.size()).device(device)();
  lci::rmr_t rmr = lci::get_rmr(mr);
  std::vector<lci::rmr_t> all_rmrs(nranks);
  lci::allgather_x(&rmr, all_rmrs.data(), sizeof(rmr)).device(device)();
  lci::rmr_t peer_rmr = all_rmrs[peer_rank];

  lci::comp_t remote_comp = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(remote_comp);

  lci::barrier_x().device(device)();
  run_pingpong(g_config.warmup_iters, is_initiator, peer_rank, send_buffer, mr,
               peer_rmr, device, remote_comp, rcomp);
  lci::barrier_x().device(device)();

  auto start = std::chrono::high_resolution_clock::now();
  run_pingpong(g_config.niters, is_initiator, peer_rank, send_buffer, mr,
               peer_rmr, device, remote_comp, rcomp);
  lci::barrier_x().device(device)();
  auto end = std::chrono::high_resolution_clock::now();
  lci::wait_drained_x().device(device)();

  for (size_t i = 0; i < g_config.msg_size; ++i) {
    assert(static_cast<unsigned char*>(recv_buffer)[i] ==
           static_cast<unsigned char>(peer_rank + 1));
  }

  if (rank == 0) {
    std::chrono::duration<double> elapsed = end - start;
    double round_trip_latency =
        elapsed.count() / static_cast<double>(g_config.niters);
    double put_latency = round_trip_latency / 2;
    std::cout << "Running " << g_config.msg_size << "-byte "
              << (g_config.force_posted_writedata ? "forced posted writedata"
                                                  : "normal remote completion")
              << " PUT ping-pong for " << g_config.niters
              << " measured iterations" << std::endl;
    std::cout << "Average round-trip latency: " << round_trip_latency * 1e6
              << " us" << std::endl;
    std::cout << "Average PUT latency: " << put_latency * 1e6 << " us"
              << std::endl;
    std::cout << "Message rate: "
              << 2 * static_cast<double>(g_config.niters) / elapsed.count() /
                     1e6
              << " Mmsgs/s" << std::endl;
  }

  lci::free_comp(&remote_comp);
  lci::deregister_memory(&mr);
  lci::free_device(&device);
  lci::g_runtime_fina();
  lci::global_finalize();
  return 0;
}
