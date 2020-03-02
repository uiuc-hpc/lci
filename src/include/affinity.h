#ifndef AFFINITY_H_
#define AFFINITY_H_

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

LC_INLINE int set_me_to(int core_id)
{
#ifdef USE_AFFI
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

#ifdef AFF_DEBUG
  fprintf(stderr, "[USE_AFFI] Setting someone to core # %d\n", core_id);
#endif
  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#else
  return 0;
#endif
}

LC_INLINE int set_me_within(int from, int to)
{
#ifdef USE_AFFI
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  int i;
  for (i = from; i < to; i++) CPU_SET(i, &cpuset);
  // std::cerr << "[USE_AFFI] Setting someone to core #[" << from << " - " << to
  // <<")" << std::endl;
  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#else
  return 0;
#endif
}

#endif
