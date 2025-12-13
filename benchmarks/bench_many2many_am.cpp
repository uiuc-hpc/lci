// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#include <atomic>
#include <vector>

#include <cxxopts.hpp>
#include <omp.h>

#include "lci.hpp"

struct config_t {
  size_t msg_size = 1024;
  size_t nmsgs_per_rank = 1000;
  size_t max_pending_msgs = 128;
  uint64_t seed = 1;
  int nthreads = 1;
  int ndevices = -1;
  bool use_upacket = false;
  bool touch_buffers = false;
} g_config;

static std::atomic<uint64_t> g_pending{0};
static std::atomic<uint64_t> g_received{0};
static std::atomic<uint64_t> g_payload_checksum{0};

static void reduce_max_double(const void* left, const void* right, void* dst,
                              size_t n)
{
  const double* l = static_cast<const double*>(left);
  const double* r = static_cast<const double*>(right);
  double* d = static_cast<double*>(dst);
  for (size_t i = 0; i < n; ++i) {
    d[i] = std::max(l[i], r[i]);
  }
}

static void reduce_sum_uint64(const void* left, const void* right, void* dst,
                              size_t n)
{
  const uint64_t* l = static_cast<const uint64_t*>(left);
  const uint64_t* r = static_cast<const uint64_t*>(right);
  uint64_t* d = static_cast<uint64_t*>(dst);
  for (size_t i = 0; i < n; ++i) {
    d[i] = l[i] + r[i];
  }
}

int main(int argc, char** argv)
{
  cxxopts::Options options("lci_many2many_am",
                           "Many-to-many active message bandwidth benchmark");
  options.add_options()("s,msg-size", "Active message payload size (bytes)",
                        cxxopts::value<size_t>()->default_value(
                            std::to_string(g_config.msg_size)))(
      "n,nmsgs",
      "Number of active messages each rank sends (messages per rank)",
      cxxopts::value<size_t>()->default_value(
          std::to_string(g_config.nmsgs_per_rank)))(
      "m,max-pending-msgs",
      "Maximum number of pending active messages (0 => unlimited)",
      cxxopts::value<size_t>()->default_value(
          std::to_string(g_config.max_pending_msgs)))(
      "t,nthreads", "Number of threads",
      cxxopts::value<int>()->default_value(
          std::to_string(g_config.nthreads)))(
      "d,ndevices", "Number of devices (-1 => same as nthreads)",
      cxxopts::value<int>()->default_value(
          std::to_string(g_config.ndevices)))(
      "r,seed", "Random seed base", cxxopts::value<uint64_t>()->default_value(
                                        std::to_string(g_config.seed)))(
      "u,use-upacket", "Use upacket for AM payloads",
      cxxopts::value<bool>()->default_value("false"))(
      "b,touch-buffers",
      "Read and write AM message buffers to stress the memory system",
      cxxopts::value<bool>()->default_value("false"))(
      "h,help", "Print help");

  const auto result = options.parse(argc, argv);
  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  g_config.msg_size = result["msg-size"].as<size_t>();
  g_config.nmsgs_per_rank = result["nmsgs"].as<size_t>();
  g_config.max_pending_msgs = result["max-pending-msgs"].as<size_t>();
  g_config.seed = result["seed"].as<uint64_t>();
  g_config.nthreads = result["nthreads"].as<int>();
  g_config.ndevices = result["ndevices"].as<int>();
  g_config.use_upacket = result["use-upacket"].as<bool>();
  g_config.touch_buffers = result["touch-buffers"].as<bool>();
  if (g_config.ndevices == -1) {
    g_config.ndevices = g_config.nthreads;
  }

  lci::g_runtime_init_x().alloc_default_device(false)();

  const int rank = lci::get_rank_me();
  const int nranks = lci::get_rank_n();

  if (rank == 0) {
    if (g_config.msg_size > lci::get_max_bcopy_size()) {
      std::cerr << "Warning: message size " << g_config.msg_size
                << " exceeds the maximum AM payload size "
                << lci::get_max_bcopy_size() << "!\n";
      if (g_config.use_upacket) {
        std::cerr << "Error: cannot use upacket for AM payloads larger than the "
                     "maximum AM payload size.\n";
        std::exit(EXIT_FAILURE);
      }
      if (g_config.max_pending_msgs == 0 || g_config.max_pending_msgs > 1024) {
        std::cerr << "Warning: too many pending rendezvous messages may crash the network driver's memory registration.\n";
      }
    }
  }

  // Allocate devices
  std::vector<lci::device_t> devices(g_config.ndevices);
  for (auto& dev : devices) {
    dev = lci::alloc_device();
  }

  // Prepare random destinations and the number of messages sent to each peer.
  std::mt19937_64 gen(g_config.seed + static_cast<uint64_t>(rank));
  std::uniform_int_distribution<int> dist(0, nranks - 1);
  std::vector<int> targets;
  targets.reserve(g_config.nmsgs_per_rank);
  std::vector<uint64_t> send_counts(nranks, 0);
  for (size_t i = 0; i < g_config.nmsgs_per_rank; ++i) {
    const int dst = dist(gen);
    targets.push_back(dst);
    send_counts[dst]++;
  }

  // Allreduce to figure out how many messages each rank should receive.
  std::vector<uint64_t> recv_counts(nranks, 0);
  lci::allreduce_x(send_counts.data(), recv_counts.data(), recv_counts.size(),
                   sizeof(uint64_t), reduce_sum_uint64).device(devices[0])();
  const uint64_t expected_recv = recv_counts[rank];

  g_received.store(0, std::memory_order_relaxed);
  g_pending.store(0, std::memory_order_relaxed);
  // Function to invoke when send is complete
  auto free_handler_fn = [](lci::status_t status) {
    if (!g_config.use_upacket)
      std::free(status.buffer);
    // else: upacket will be automatically returned once send completes
    g_pending.fetch_sub(1, std::memory_order_relaxed);
  };
  lci::comp_t free_handler = lci::alloc_handler(free_handler_fn);
  // Function to invoke when an active message is received
  auto am_handler_fn = [](lci::status_t status) {
    g_received.fetch_add(1, std::memory_order_relaxed);
    if (g_config.touch_buffers && status.buffer && status.size > 0) {
      const auto* bytes = static_cast<const std::uint8_t*>(status.buffer);
      uint64_t checksum = 0;
      for (size_t i = 0; i < status.size; ++i) {
        checksum += bytes[i];
      }
      g_payload_checksum.fetch_add(checksum, std::memory_order_relaxed);
    }
    if (g_config.use_upacket) {
      lci::put_upacket(status.buffer);
    } else {
      std::free(status.buffer);
    }
  };
  lci::comp_t am_handler = lci::alloc_handler_x(am_handler_fn).zero_copy_am(g_config.use_upacket)();
  const lci::rcomp_t rcomp = lci::register_rcomp(am_handler);

  lci::barrier_x().device(devices[0])();
  const auto start = std::chrono::high_resolution_clock::now();

#pragma omp parallel num_threads(g_config.nthreads)
  {
    const int tid = omp_get_thread_num();
    const lci::device_t device = devices[tid % g_config.ndevices];
    const size_t msgs_per_thread =
        (g_config.nmsgs_per_rank + g_config.nthreads - 1) /
        g_config.nthreads;
    const size_t start_idx = msgs_per_thread * tid;
    const size_t end_idx =
        std::min(g_config.nmsgs_per_rank, start_idx + msgs_per_thread);

    for (size_t i = start_idx; i < end_idx; ++i) {
      if (g_config.max_pending_msgs > 0)
        while (g_pending.load(std::memory_order_relaxed) >= g_config.max_pending_msgs) {
          lci::progress_x().device(device)();
        }
      g_pending.fetch_add(1, std::memory_order_relaxed);
      void *payload = nullptr;
      if (g_config.use_upacket) {
        while (payload == nullptr) {
          payload = lci::get_upacket();
          if (payload == nullptr) {
            lci::progress_x().device(device)();
          }
        }
      } else {
        payload = std::malloc(g_config.msg_size);
      }
      if (g_config.touch_buffers && payload && g_config.msg_size > 0) {
        auto* bytes = static_cast<std::uint8_t*>(payload);
        const std::uint8_t pattern =
            static_cast<std::uint8_t>((rank + i) & 0xFF);
        std::fill_n(bytes, g_config.msg_size, pattern);
      }
      lci::status_t status;
      do {
        status = lci::post_am_x(targets[i], payload,
                                g_config.msg_size, free_handler,
                                rcomp)
                     .device(device)();
        if (status.is_retry()) {
          lci::progress_x().device(device)();
        }
      } while (status.is_retry());
      if (status.is_done()) {
        free_handler_fn(status);
      }
      lci::progress_x().device(device)();
    }
    // fprintf(stderr, "Rank %d thread %d sent %zu messages\n", rank, tid,
    //         end_idx - start_idx);
    while (g_received.load(std::memory_order_relaxed) < expected_recv) {
      lci::progress_x().device(device)();
    }
    // fprintf(stderr, "Rank %d thread %d received all %zu messages\n", rank,
            // tid, expected_recv);
    lci::wait_drained_x().device(device)();
  }

  const auto end = std::chrono::high_resolution_clock::now();

  lci::barrier_x().device(devices[0])();

  lci::deregister_rcomp(rcomp);
  lci::free_comp(&free_handler);
  lci::free_comp(&am_handler);

  const std::chrono::duration<double> elapsed = end - start;
  double elapsed_sec = elapsed.count();
  double max_elapsed = 0.0;
  lci::allreduce_x(&elapsed_sec, &max_elapsed, 1, sizeof(double),
                   reduce_max_double).device(devices[0])();
  uint64_t local_checksum = g_payload_checksum.load(std::memory_order_relaxed);
  uint64_t global_checksum = 0;
  lci::allreduce_x(&local_checksum, &global_checksum, 1, sizeof(uint64_t),
                   reduce_sum_uint64).device(devices[0])();
  const double total_msgs_global =
      static_cast<double>(g_config.nmsgs_per_rank) * nranks;
  const double total_bytes_global =
      total_msgs_global * static_cast<double>(g_config.msg_size);
  if (rank == 0) {
    const double msg_rate_m_global = total_msgs_global / max_elapsed / 1e6;
    const double bandwidth_gb_global = total_bytes_global / max_elapsed / 1e9;
    std::cout << "Total ranks: " << nranks << "\n";
    std::cout << "Message size (per AM payload): " << g_config.msg_size
              << " bytes\n";
    std::cout << "Messages per rank (sent): " << g_config.nmsgs_per_rank
              << "\n";
    std::cout << "Total messages (all ranks sent): " << total_msgs_global
              << "\n";
    std::cout << "Elapsed time (max across ranks): " << max_elapsed << " s\n";
    std::cout << "Global message rate (all ranks aggregated): "
              << msg_rate_m_global << " Mmsgs/s\n";
    std::cout << "Global achieved bandwidth (all ranks aggregated): "
              << bandwidth_gb_global << " GB/s\n";
    if (g_config.touch_buffers) {
      std::cout << "Global payload checksum: " << global_checksum << "\n";
    }
  }

  for (auto& dev : devices) {
    lci::free_device(&dev);
  }
  lci::g_runtime_fina();
  return 0;
}
