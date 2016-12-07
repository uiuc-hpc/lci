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
#include <stdio.h>

#include "macro.h"
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#define SERVER_CORE (get_ncores() - 1)

MV_INLINE int get_ncores()
{
  int logical = sysconf( _SC_NPROCESSORS_ONLN );
#if 0
  uint32_t registers[4];
  __asm__ __volatile__ ("cpuid " :
          "=a" (registers[0]),
          "=b" (registers[1]),
          "=c" (registers[2]),
          "=d" (registers[3])
          : "a" (1), "c" (0));
  unsigned cpufset = registers[3];
  if (cpufset & (1 << 28)) { // hyperthreading
      return logical / 2;
  } else {
      return logical;
  }
#endif
  return logical;
}

MV_INLINE int set_me_to_(int core_id)
{
  int num_cores = get_ncores();
  if (core_id < 0 || core_id >= num_cores) return EINVAL;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  fprintf(stderr, "[USE_AFFI] Setting someone to core # %d\n", core_id);
  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

MV_INLINE int set_me_within(int from, int to)
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

MV_INLINE int set_me_to(int core_id)
{
  int num_cores = get_ncores();
  // TODO(danghvu): do this because the second set is near mlx
  return set_me_to_((SERVER_CORE - 1 - core_id + num_cores) % num_cores);
}

MV_INLINE int set_me_to_last()
{
  return set_me_to_(SERVER_CORE);
}

#endif

#endif
