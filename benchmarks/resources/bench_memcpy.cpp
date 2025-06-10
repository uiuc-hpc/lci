// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <getopt.h>
#include <thread>
#include <chrono>

#include "lct.h"
#include "lci.hpp"

#include "util.hpp"

struct config_t {
  int nthreads = 16;
  int niters = 1;
  size_t total_size = 1024 * 1024 * 1024; // Total size in bytes
  int block_size = 64;
} config;

LCT_tbarrier_t g_tbarrier;

char *src_buffer;
char *dst_buffer;

void worker(int id) {
  util::pin_thread_to_cpu(id);
  size_t total_accesses = config.total_size / config.block_size;
  size_t accesses_per_thread = total_accesses / config.nthreads;
  size_t start_access = id * accesses_per_thread;
  LCT_tbarrier_arrive_and_wait(g_tbarrier);
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < config.niters; i++) {
    for (int j = 0; j < accesses_per_thread; j++) {
      size_t access = start_access + j;
      size_t offset = access * config.block_size;
      memcpy(dst_buffer + offset, src_buffer + offset, config.block_size);
    }
  }
  LCT_tbarrier_arrive_and_wait(g_tbarrier);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  double elapsed_s = elapsed.count();
  if (id == 0) {
    printf("Elapsed time: %.2f s\n", elapsed_s);
    printf("Per-operation time: %.2f us\n",
           (elapsed_s * 1e6) / (config.niters * accesses_per_thread));
    double throughput_per_thread = (static_cast<double>(config.niters) * accesses_per_thread) / (elapsed_s * 1e6);
    printf("Throughput per thread: %.2f Mops/s\n",
           throughput_per_thread);
    printf("Throughput: %.2f Mops/s\n", throughput_per_thread * config.nthreads);
    printf("Bandwidth per thread: %.2f GB/s\n",
           (throughput_per_thread * config.block_size) / 1e3);
    printf("Bandwidth: %.2f GB/s\n",
           (throughput_per_thread * config.nthreads * config.block_size) / 1e3);
  }
}

int main(int argc, char** argv) {
  int total_size_mb = config.total_size / (1024 * 1024);
  LCT_args_parser_t argsParser = LCT_args_parser_alloc();
  LCT_args_parser_add(argsParser, "nthreads", required_argument,
    &config.nthreads);
  LCT_args_parser_add(argsParser, "niters", required_argument,
      &config.niters);
  LCT_args_parser_add(argsParser, "total-size", required_argument,
      &total_size_mb);
  LCT_args_parser_add(argsParser, "block-size", required_argument,
      &config.block_size);
  LCT_args_parser_parse(argsParser, argc, argv);
  LCT_args_parser_print(argsParser, true);
  LCT_args_parser_free(argsParser);

  config.total_size = static_cast<size_t>(total_size_mb) * 1024 * 1024; // Convert to bytes
  g_tbarrier = LCT_tbarrier_alloc(config.nthreads);
  src_buffer = static_cast<char*>(aligned_alloc(4096, config.total_size));
  if (src_buffer == nullptr) {
    fprintf(stderr, "malloc failed\n");
    exit(1);
  }
  dst_buffer = static_cast<char*>(aligned_alloc(4096, config.total_size));
  if (dst_buffer == nullptr) {
    fprintf(stderr, "malloc failed\n");
    exit(1);
  }

  lci::g_runtime_init();

  std::vector<std::thread> threads;
  for (int i = 0; i < config.nthreads; i++) {
    std::thread t(worker, i);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::g_runtime_fina();

  free(src_buffer);
  free(dst_buffer);

  LCT_tbarrier_free(&g_tbarrier);
  return 0;
}