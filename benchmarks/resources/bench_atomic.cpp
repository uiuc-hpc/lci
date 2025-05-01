#include <getopt.h>
#include <thread>
#include <chrono>
#include <atomic>

#include "lct.h"
#include "lci.hpp"

#include "util.hpp"

struct config_t {
  int nthreads = 16;
  int niters = 1000;
} config;

LCT_tbarrier_t g_tbarrier;
std::atomic<uint64_t> g_counter = 0;

void worker(int id) {
  util::pin_thread_to_cpu(id);
  LCT_tbarrier_arrive_and_wait(g_tbarrier);
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < config.niters; i++) {
    g_counter.fetch_add(1, std::memory_order_relaxed);
  }
  LCT_tbarrier_arrive_and_wait(g_tbarrier);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  double elapsed_s = elapsed.count();
  if (id == 0) {
    printf("Elapsed time: %.2f s\n", elapsed_s);
    printf("Per-operation time: %.2f us\n",
           (elapsed_s * 1e6) / config.niters);
    double throughput_per_thread = static_cast<double>(config.niters) / (elapsed_s * 1e6);
    printf("Throughput per thread: %.2f Mops/s\n",
           throughput_per_thread);
    printf("Throughput: %.2f Mops/s\n", throughput_per_thread * config.nthreads);
  }
}

int main(int argc, char** argv) {
  LCT_args_parser_t argsParser = LCT_args_parser_alloc();
  LCT_args_parser_add(argsParser, "nthreads", required_argument,
    &config.nthreads);
  LCT_args_parser_add(argsParser, "niters", required_argument,
      &config.niters);
  LCT_args_parser_parse(argsParser, argc, argv);
  LCT_args_parser_print(argsParser, true);
  LCT_args_parser_free(argsParser);

  g_tbarrier = LCT_tbarrier_alloc(config.nthreads);

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

  LCT_tbarrier_free(&g_tbarrier);
  return 0;
}