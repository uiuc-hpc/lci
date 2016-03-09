#ifndef AFFINITY_H_
#define AFFINITY_H_

#include "config.h"

#ifdef USE_AFFI

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>

namespace affinity {
  static int set_me_to_(int core_id) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores)
      return EINVAL;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    std::cerr << "[USE_AFFI] Setting someone to core #" << core_id << std::endl;
    pthread_t current_thread = pthread_self();    
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
  }

  static int set_me_within(int from, int to) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = from; i < to; i++)
      CPU_SET(i, &cpuset);
    std::cerr << "[USE_AFFI] Setting someone to core #[" << from << " - " << to <<")" << std::endl;
    pthread_t current_thread = pthread_self();    
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
  }

  static int set_me_to(int core_id) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    // TODO(danghvu): do this because the second set is near mlx
    return set_me_to_(num_cores - core_id - 2);
  }

  static int set_me_to_last() {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    return set_me_to_(num_cores - 1);
  }

}

#endif

#endif
