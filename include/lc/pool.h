#ifndef LC_POOL_H_
#define LC_POOL_H_

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "thread.h"
#include "lc/lock.h"
#include "lc/dequeue.h"
#include "ptmalloc.h"

#define MAX_NPOOLS 128
#define MAX_LOCAL_POOL 8  // align to a cache line.

extern int lc_pool_nkey;
extern int32_t tls_pool_struct[MAX_NPOOLS][MAX_LOCAL_POOL];
extern volatile int init_lock;

struct dequeue;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lc_pool {
  int key;
  int32_t npools;
  struct dequeue* lpools[MAX_NPOOLS];
} lc_pool __attribute__((aligned(64)));

LC_EXPORT
void lc_pool_create(lc_pool** pool);

LC_EXPORT
void lc_pool_destroy(lc_pool* pool);

LC_EXPORT
void lc_pool_put(lc_pool* pool, void* elm);

LC_EXPORT
void lc_pool_put_to(lc_pool* pool, void* elm, int32_t pid);

LC_EXPORT
void* lc_pool_get(lc_pool* pool);

LC_EXPORT
void* lc_pool_get_nb(lc_pool* pool);

#define POOL_EMPTY (NULL)

#ifdef __cplusplus
}
#endif

#define POOL_UNINIT ((int32_t)-1)

LC_INLINE int32_t lc_pool_get_local(struct lc_pool* pool)
{
  int wid = lc_worker_id();
  int32_t pid = tls_pool_struct[wid][pool->key];
  if (unlikely(pid == POOL_UNINIT)) {
    lc_spin_lock(&init_lock);
    pid = tls_pool_struct[wid][pool->key];
    if (pid == POOL_UNINIT) {
      struct dequeue* lpool =
          (struct dequeue*)memalign(64, sizeof(struct dequeue));
      dq_init(lpool);
      pid = pool->npools++;
      pool->lpools[pid] = lpool;
      tls_pool_struct[wid][pool->key] = pid;
    }
    lc_spin_unlock(&init_lock);
  }
  // assert(pid >= 0 && pid < pool->npools && "POOL ERROR: pid out-of-range");
  return pid;
}

#endif
