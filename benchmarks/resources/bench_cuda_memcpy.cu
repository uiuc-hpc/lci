#include <getopt.h>
#include <thread>
#include <chrono>
#include <cuda_runtime.h>

#include "lct.h"
#include "lci.hpp"

#include "util.hpp"

#define CUDA_CHECK(call)                                                \
  do {                                                                  \
    cudaError_t err = call;                                             \
    if (cudaSuccess != err) {                                           \
      fprintf(stderr, "Cuda failure %s:%d: '%s'\n", __FILE__, __LINE__, \
              cudaGetErrorString(err));                                 \
      exit(EXIT_FAILURE);                                               \
    }                                                                   \
  } while (0)

enum class memcpy_type_t {
  HOST_TO_HOST,
  HOST_TO_DEVICE,
  DEVICE_TO_HOST,
  DEVICE_TO_DEVICE,
};

struct config_t {
  int nthreads = 16;
  int niters = 1;
  size_t total_size = 1024 * 1024 * 1024; // Total size in bytes
  int block_size = 4096;
  memcpy_type_t memcpy_type = memcpy_type_t::HOST_TO_DEVICE;
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
      switch (config.memcpy_type) {
        case memcpy_type_t::HOST_TO_HOST:
          memcpy(dst_buffer + offset, src_buffer + offset, config.block_size);
          break;
        case memcpy_type_t::HOST_TO_DEVICE:
          CUDA_CHECK(cudaMemcpy(dst_buffer + offset, src_buffer + offset, config.block_size, cudaMemcpyHostToDevice));
          break;
        case memcpy_type_t::DEVICE_TO_HOST:
          CUDA_CHECK(cudaMemcpy(dst_buffer + offset, src_buffer + offset, config.block_size, cudaMemcpyDeviceToHost));
          break;
        case memcpy_type_t::DEVICE_TO_DEVICE:
          CUDA_CHECK(cudaMemcpy(dst_buffer + offset, src_buffer + offset, config.block_size, cudaMemcpyDeviceToDevice));
      }
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
  LCT_dict_str_int_t memcpy_type_dict[] = {
    {"h2h", static_cast<int>(memcpy_type_t::HOST_TO_HOST)},
    {"h2d", static_cast<int>(memcpy_type_t::HOST_TO_DEVICE)},
    {"d2h", static_cast<int>(memcpy_type_t::DEVICE_TO_HOST)},
    {"d2d", static_cast<int>(memcpy_type_t::DEVICE_TO_DEVICE)},
  };
  LCT_args_parser_add_dict(argsParser, "type", required_argument,
      reinterpret_cast<int*>(&config.memcpy_type), memcpy_type_dict, 4);
  LCT_args_parser_parse(argsParser, argc, argv);
  LCT_args_parser_print(argsParser, true);
  LCT_args_parser_free(argsParser);

  config.total_size = static_cast<size_t>(total_size_mb) * 1024 * 1024; // Convert to bytes
  g_tbarrier = LCT_tbarrier_alloc(config.nthreads);
  switch (config.memcpy_type) {
    case memcpy_type_t::HOST_TO_HOST:
      src_buffer = static_cast<char*>(aligned_alloc(4096, config.total_size));
      dst_buffer = static_cast<char*>(aligned_alloc(4096, config.total_size));
      break;
    case memcpy_type_t::HOST_TO_DEVICE:
      src_buffer = static_cast<char*>(aligned_alloc(4096, config.total_size));
      CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dst_buffer), config.total_size));
      break;
    case memcpy_type_t::DEVICE_TO_HOST:
      CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&src_buffer), config.total_size));
      dst_buffer = static_cast<char*>(aligned_alloc(4096, config.total_size));
      break;
    case memcpy_type_t::DEVICE_TO_DEVICE:
      CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&src_buffer), config.total_size));
      CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&dst_buffer), config.total_size));
      break;
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

  switch (config.memcpy_type) {
    case memcpy_type_t::HOST_TO_HOST:
      free(src_buffer);
      free(dst_buffer);
      break;
    case memcpy_type_t::HOST_TO_DEVICE:
      free(src_buffer);
      CUDA_CHECK(cudaFree(dst_buffer));
      break;
    case memcpy_type_t::DEVICE_TO_HOST:
      CUDA_CHECK(cudaFree(src_buffer));
      free(dst_buffer);
      break;
    case memcpy_type_t::DEVICE_TO_DEVICE:
      CUDA_CHECK(cudaFree(src_buffer));
      CUDA_CHECK(cudaFree(dst_buffer));
      break;
  }

  LCT_tbarrier_free(&g_tbarrier);
  return 0;
}