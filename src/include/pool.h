#ifndef MV_POOL_H_
#define MV_POOL_H_

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dequeue.h"
#include "mv/ult/ult.h"

#define MAX_NPOOLS 128
#define MAX_LOCAL_POOL 8  // align to a cache line.

extern int mv_pool_nkey;
extern int32_t tls_pool_struct[MAX_NPOOLS][MAX_LOCAL_POOL];

typedef struct mv_pool {
  int key;
  int32_t npools;
  struct dequeue* lpools[MAX_NPOOLS];
} mv_pool __attribute__((aligned(64)));

void mv_pool_init();
void mv_pool_create(mv_pool** pool);
void mv_pool_destroy(mv_pool* pool);
void mv_pool_put(mv_pool* pool, void* elm);
void mv_pool_put_to(mv_pool* pool, void* elm, int32_t pid);
void* mv_pool_get(mv_pool* pool);
void* mv_pool_get_nb(mv_pool* pool);
void* mv_pool_get_slow(mv_pool* pool);

extern volatile int init_pool_lock;

#define EMPTY_POOL ((int32_t)-1)

MV_INLINE int32_t mv_pool_get_local(mv_pool* pool)
{
  int wid = mv_worker_id();
  int32_t pid = tls_pool_struct[wid][pool->key];
  if (unlikely(pid == EMPTY_POOL)) {
    mv_spin_lock(&init_pool_lock);
    pid = tls_pool_struct[wid][pool->key];
    if (pid == EMPTY_POOL) {
      struct dequeue* lpool = 0;
      posix_memalign((void**)&lpool, 64, sizeof(struct dequeue));
      // assert(lpool != 0 && "POOL ERROR: Unable to create dequeue");
      dq_init(lpool);
      pid = pool->npools++;
      pool->lpools[pid] = lpool;
      mv_mem_fence();
      tls_pool_struct[wid][pool->key] = pid;
    }
    mv_spin_unlock(&init_pool_lock);
  }
  // assert(pid >= 0 && pid < pool->npools && "POOL ERROR: pid out-of-range");
  return pid;
}

#endif
