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
  int niters = 1000;
  int window = 1;
} config;

LCT_tbarrier_t g_tbarrier;

void worker(int id, lci::comp_t cq) {
  util::pin_thread_to_cpu(id);
  lci::status_t dummy_status;
  dummy_status.set_done();
  LCT_tbarrier_arrive_and_wait(g_tbarrier);
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < config.niters; i++) {
    for (int j = 0; j < config.window; j++) {
      lci::comp_signal(cq, dummy_status);
    }
    for (int j = config.window - 1; j >= 0; j--) {
      lci::status_t status;
      do {
        status = lci::cq_pop(cq);
      } while (status.is_retry());
    }
  }
  LCT_tbarrier_arrive_and_wait(g_tbarrier);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  double elapsed_s = elapsed.count();
  if (id == 0) {
    printf("Elapsed time: %.2f s\n", elapsed_s);
    printf("Throughput: %.2f Mops/s\n",
           (config.nthreads * config.niters * config.window) / (elapsed_s * 1e6));
  }
}

int main(int argc, char** argv) {
  LCT_args_parser_t argsParser = LCT_args_parser_alloc();
  LCT_args_parser_add(argsParser, "nthreads", required_argument,
    &config.nthreads);
  LCT_args_parser_add(argsParser, "niters", required_argument,
      &config.niters);
      LCT_args_parser_add(argsParser, "window", required_argument,
          &config.window);
  LCT_args_parser_parse(argsParser, argc, argv);
  LCT_args_parser_print(argsParser, true);
  LCT_args_parser_free(argsParser);

  g_tbarrier = LCT_tbarrier_alloc(config.nthreads);

  lci::g_runtime_init();
  lci::comp_t cq = lci::alloc_cq();

  std::vector<std::thread> threads;
  for (int i = 0; i < config.nthreads; i++) {
    std::thread t(worker, i, cq);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::free_comp(&cq);
  lci::g_runtime_fina();

  LCT_tbarrier_free(&g_tbarrier);
  return 0;
}