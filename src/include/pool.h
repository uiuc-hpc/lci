#ifndef LC_POOL_H_
#define LC_POOL_H_

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_NPOOLS 272
#define MAX_LOCAL_POOL 32  // align to a cache line.

extern int lc_pool_nkey;
extern int32_t tls_pool_struct[MAX_NPOOLS][MAX_LOCAL_POOL];
extern LCIU_spinlock_t init_lock;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  LCIU_spinlock_t lock; // size 4 align 4
  LCM_dequeue_t dq;     // size 32 align 8
  char padding[24];
} LCII_local_pool_t __attribute__((aligned(64)));

typedef struct lc_pool {
  int key;
  int npools;
  LCII_local_pool_t lpools[MAX_NPOOLS] __attribute__((aligned(64)));
} lc_pool __attribute__((aligned(64)));

void lc_pool_create(lc_pool** pool);
void lc_pool_destroy(lc_pool* pool);
int lc_pool_count(const struct lc_pool* pool);
static inline void lc_pool_put(lc_pool* pool, void* elm);
static inline void lc_pool_put_to(lc_pool* pool, void* elm, int32_t pid);
static inline void* lc_pool_get(lc_pool* pool);
static inline void* lc_pool_get_nb(lc_pool* pool);

#ifdef __cplusplus
}
#endif

#define POOL_UNINIT ((int32_t)-1)

/*
 * We would like to hide these two global variable,
 * but we cannot do it easily, because:
 * - we want to make LCIU_thread_id an inline function
 */
extern int LCIU_nthreads;
extern __thread int LCIU_thread_id;

/* thread id */
static inline int LCIU_get_thread_id()
{
  if (unlikely(LCIU_thread_id == -1)) {
    LCIU_thread_id = sched_getcpu();
    if (LCIU_thread_id == -1) {
      LCIU_thread_id = __sync_fetch_and_add(&LCIU_nthreads, 1);
    }
  }
  return LCIU_thread_id;
}

static inline int32_t lc_pool_get_local(struct lc_pool* pool)
{
  int wid = LCIU_get_thread_id();
  int32_t pid = tls_pool_struct[wid][pool->key];
  if (unlikely(pid == POOL_UNINIT)) {
    LCIU_acquire_spinlock(&init_lock);
    pid = tls_pool_struct[wid][pool->key];
    if (pid == POOL_UNINIT) {
      pid = pool->npools++;
      LCIU_spinlock_init(&pool->lpools[pid].lock);
      LCM_dq_init(&pool->lpools[pid].dq, LC_SERVER_NUM_PKTS);
      tls_pool_struct[wid][pool->key] = pid;
    }
    LCIU_release_spinlock(&init_lock);
  }
  // assert(pid >= 0 && pid < pool->npools && "POOL ERROR: pid out-of-range");
  return pid;
}

// TODO: improve this
static inline void* lc_pool_get_slow(struct lc_pool* pool)
{
  void *ret = NULL;
  int steal = rand() % (pool->npools);
  LCIU_acquire_spinlock(&pool->lpools[steal].lock);
  ret = LCM_dq_pop_bot(&pool->lpools[steal].dq);
  LCIU_release_spinlock(&pool->lpools[steal].lock);
  return ret;
}

static inline void lc_pool_put_to(struct lc_pool* pool, void* elm, int32_t pid)
{
  LCIU_acquire_spinlock(&pool->lpools[pid].lock);
  LCM_dq_push_top(&pool->lpools[pid].dq, elm);
  LCIU_release_spinlock(&pool->lpools[pid].lock);
}

static inline void lc_pool_put(struct lc_pool* pool, void* elm)
{
  int32_t pid = lc_pool_get_local(pool);
  lc_pool_put_to(pool, elm, pid);
}

static inline void* lc_pool_get(struct lc_pool* pool)
{
  int32_t pid = lc_pool_get_local(pool);
  LCIU_acquire_spinlock(&pool->lpools[pid].lock);
  void* elm = LCM_dq_pop_top(&pool->lpools[pid].dq);
  LCIU_release_spinlock(&pool->lpools[pid].lock);
  while (elm == NULL)
    elm = lc_pool_get_slow(pool);
  return elm;
}

static inline void* lc_pool_get_nb(struct lc_pool* pool)
{
  int32_t pid = lc_pool_get_local(pool);
  LCIU_acquire_spinlock(&pool->lpools[pid].lock);
  void* elm = LCM_dq_pop_top(&pool->lpools[pid].dq);
  LCIU_release_spinlock(&pool->lpools[pid].lock);
  if (elm == NULL)
    elm = lc_pool_get_slow(pool);
  return elm;
}

#endif // LC_POOL_H_
