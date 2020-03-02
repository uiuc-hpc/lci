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
#include "lock.h"
#include "dequeue.h"

#define MAX_NPOOLS 272
#define MAX_LOCAL_POOL 32  // align to a cache line.

extern int lc_pool_nkey;
extern int32_t tls_pool_struct[MAX_NPOOLS][MAX_LOCAL_POOL];
extern volatile int init_lock;

struct dequeue;

#ifdef __cplusplus
extern "C" {
#endif

LC_INLINE int lc_worker_id()
{
  if (unlikely(lcg_core_id == -1)) {
    lcg_core_id = sched_getcpu();
    if (lcg_core_id == -1) {
      lcg_core_id = __sync_fetch_and_add(&lcg_current_id, 1);
    }
  }
  return lcg_core_id;
}

typedef struct lc_pool {
  int key;
  int32_t npools;
  struct dequeue* lpools[MAX_NPOOLS];
} lc_pool __attribute__((aligned(64)));

LC_EXPORT
void lc_pool_create(lc_pool** pool);

LC_EXPORT
void lc_pool_destroy(lc_pool* pool);

LC_INLINE
void lc_pool_put(lc_pool* pool, void* elm);

LC_INLINE
void lc_pool_put_to(lc_pool* pool, void* elm, int32_t pid);

LC_INLINE
void* lc_pool_get(lc_pool* pool);

LC_INLINE
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
      struct dequeue* lpool = 0;
      posix_memalign((void**)&lpool, 64, sizeof(struct dequeue));
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

LC_INLINE void* lc_pool_get_slow(struct lc_pool* pool)
{
  void* elm = NULL;
  while (!elm) {
    int steal = rand() % (pool->npools);
    if (likely(pool->lpools[steal] != NULL))
      elm = dq_pop_bot(pool->lpools[steal]);
  }
  return elm;
}

LC_INLINE void lc_pool_put(struct lc_pool* pool, void* elm)
{
  int32_t pid = lc_pool_get_local(pool);
  struct dequeue* lpool = pool->lpools[pid];
  dq_push_top(lpool, elm);
}

LC_INLINE void lc_pool_put_to(struct lc_pool* pool, void* elm, int32_t pid)
{
  struct dequeue* lpool = pool->lpools[pid];
  dq_push_top(lpool, elm);
}

LC_INLINE void* lc_pool_get(struct lc_pool* pool)
{
  int32_t pid = lc_pool_get_local(pool);
  struct dequeue* lpool = pool->lpools[pid];
  void* elm = NULL;
  elm = dq_pop_top(lpool);
  if (elm == NULL) elm = lc_pool_get_slow(pool);
  return elm;
}

LC_INLINE void* lc_pool_get_nb(struct lc_pool* pool)
{
  int32_t pid = lc_pool_get_local(pool);
  struct dequeue* lpool = pool->lpools[pid];
  void* elm = NULL;
  elm = dq_pop_top(lpool);
  if (elm == NULL) {
    int steal = rand() % (pool->npools);
    if (likely(pool->lpools[steal] != NULL))
      elm = dq_pop_bot(pool->lpools[steal]);
  }
  return elm;
}

#endif
