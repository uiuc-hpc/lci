#ifndef AFFINITY_H_
#define AFFINITY_H_

#ifdef USE_AFFI

#ifndef __USE_GNU
#define __USE_GNU
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sched.h>

#include "macro.h"
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#define GET_NCORE() sysconf(_SC_NPROCESSORS_ONLN)

#ifdef __cplusplus
namespace affinity
{
#endif

inline int set_me_to_(int core_id)
{
  int num_cores = GET_NCORE();
  if (core_id < 0 || core_id >= num_cores) return EINVAL;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  std::cerr << "[USE_AFFI] Setting someone to core #" << core_id << std::endl;
  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

inline int set_me_within(int from, int to)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  int i;
  for (i = from; i < to; i++) CPU_SET(i, &cpuset);
  // std::cerr << "[USE_AFFI] Setting someone to core #[" << from << " - " << to
  // <<")" << std::endl;
  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

inline int set_me_to(int core_id)
{
  int num_cores = GET_NCORE();
  // TODO(danghvu): do this because the second set is near mlx
  return set_me_to_(num_cores - core_id - 2);
}

inline int set_me_to_last()
{
  int num_cores = GET_NCORE();
  return set_me_to_(num_cores - 1);
}

#ifdef __cplusplus
}
#endif

#endif

#endif
