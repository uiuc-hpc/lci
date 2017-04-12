#ifndef LC_POOL_H_
#define LC_POOL_H_

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ult/ult.h"
#include "dequeue.h"

#define MAX_NPOOLS 512
#define MAX_LOCAL_POOL 8  // align to a cache line.

extern int lc_pool_nkey;
extern int32_t tls_pool_struct[MAX_NPOOLS][MAX_LOCAL_POOL];

typedef struct lc_pool {
  int key;
  int32_t npools;
  struct dequeue* lpools[MAX_NPOOLS];
} lc_pool __attribute__((aligned(64)));

void lc_pool_init();
void lc_pool_create(lc_pool** pool);
void lc_pool_destroy(lc_pool* pool);
void lc_pool_put(lc_pool* pool, void* elm);
void lc_pool_put_to(lc_pool* pool, void* elm, int32_t pid);
void* lc_pool_get(lc_pool* pool);
void* lc_pool_get_nb(lc_pool* pool);
void* lc_pool_get_slow(lc_pool* pool);

extern volatile int init_pool_lock;

#define EMPTY_POOL ((int32_t)-1)

LC_INLINE int32_t lc_pool_get_local(lc_pool* pool)
{
  int wid = lc_worker_id();
  int32_t pid = tls_pool_struct[wid][pool->key];
  if (unlikely(pid == EMPTY_POOL)) {
    lc_spin_lock(&init_pool_lock);
    pid = tls_pool_struct[wid][pool->key];
    if (pid == EMPTY_POOL) {
      struct dequeue* lpool = 0;
      posix_memalign((void**)&lpool, 64, sizeof(struct dequeue));
      // assert(lpool != 0 && "POOL ERROR: Unable to create dequeue");
      dq_init(lpool);
      pid = pool->npools++;
      pool->lpools[pid] = lpool;
      lc_mem_fence();
      tls_pool_struct[wid][pool->key] = pid;
    }
    lc_spin_unlock(&init_pool_lock);
  }
  // assert(pid >= 0 && pid < pool->npools && "POOL ERROR: pid out-of-range");
  return pid;
}

#endif
