// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#ifndef LCI_BENCHMARKS_UTIL_HPP
#define LCI_BENCHMARKS_UTIL_HPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>

namespace util {
// TODO: Simplify the thread spawning and pinning
void pin_thread_to_cpu(size_t cpu_id) {
  // macOS does not support CPU affinity setting
#ifndef __APPLE__
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_id, &cpuset);
  pthread_t current_thread = pthread_self();
  int rc = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
    fprintf(stderr, "Error setting CPU affinity for thread %zu: %d\n", cpu_id, rc);
  }
#endif
}
} // namespace util

#endif // LCI_BENCHMARKS_UTIL_HPP