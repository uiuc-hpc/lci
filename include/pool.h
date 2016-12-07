#ifndef MV_POOL_H_
#define MV_POOL_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

#include "dequeue.h"
#include "macro.h"

#define MAX_NPOOLS 128
#define MAX_LOCAL_POOL 8

extern int mv_pool_nkey;
extern __thread int8_t tls_pool_struct[MAX_LOCAL_POOL];

typedef struct mv_pool {
  int key;
  unsigned count;
  int npools;
  void* data;
  struct dequeue* lpools[MAX_NPOOLS];
} mv_pool __attribute__((aligned(64)));

inline void mv_pool_create(mv_pool** pool, void* data, size_t elm_size, unsigned count);
inline void mv_pool_put(mv_pool* pool, void* elm);
inline void* mv_pool_get_slow(mv_pool* pool);

inline void mv_pool_create(mv_pool** pool, void* data, size_t elm_size, unsigned count) {
  mv_pool* p = 0;
  posix_memalign((void**) &p, 64, sizeof(struct mv_pool));
  p->data = data;
  p->count = count;
  p->npools = 0;
  p->key = __sync_fetch_and_add(&mv_pool_nkey, 1);
  if (p->key > MAX_LOCAL_POOL) {
    printf("Unable to allocate more pool\n");
    exit(EXIT_FAILURE);
  }
  for (unsigned i = 0; i < count; i++) {
    mv_pool_put(p, (void*) ((uintptr_t) p->data + elm_size * i));
  }
  *pool = p;
}

inline void mv_pool_destroy(mv_pool* pool) {
  for (int i = 0; i < pool->npools; i++) {
    free(pool->lpools[i]);
  }
}

MV_INLINE int8_t mv_pool_get_local(mv_pool* pool) {
  int8_t pid = tls_pool_struct[pool->key]; 
  // struct dequeue* lpool = (struct dequeue*) pthread_getspecific(pool->key);
  if (unlikely(pid == -1)) {
    struct dequeue* lpool;
    posix_memalign((void**) &lpool, 64, sizeof(struct dequeue));
    dq_init(lpool, pool->count);
    pid = __sync_fetch_and_add(&pool->npools, 1);
    tls_pool_struct[pool->key] = pid;
    pool->lpools[pid] = lpool;
  }
  return pid;
}

inline void mv_pool_put(mv_pool* pool, void* elm) {
  int8_t pid = mv_pool_get_local(pool);
  struct dequeue* lpool = pool->lpools[pid];
  if (lpool->cache == NULL) {
    lpool->cache = elm;
  } else {
    dq_push_top(lpool, elm);
  }
}

inline void mv_pool_put_to(mv_pool* pool, void* elm, int8_t pid) {
  struct dequeue* lpool = pool->lpools[pid];
  dq_push_top(lpool, elm);
}

inline void* mv_pool_get(mv_pool* pool) {
  int8_t pid = mv_pool_get_local(pool);
  struct dequeue* lpool = pool->lpools[pid];
  void *elm = NULL;
  if (lpool->cache != NULL) {
    elm = lpool->cache;
    lpool->cache = NULL;
  } else {
    elm = dq_pop_top(lpool);
    if (elm == NULL)
      elm = mv_pool_get_slow(pool);
  }
  return elm;
}

inline void* mv_pool_get_nb(mv_pool* pool) {
  int8_t pid = mv_pool_get_local(pool);
  struct dequeue* lpool = pool->lpools[pid];
  void* elm = NULL;
  if (lpool->cache != NULL) {
    elm = lpool->cache;
    lpool->cache = NULL;
  } else {
    elm = dq_pop_top(lpool);
    if (elm == NULL) {
      int steal = rand() % (pool->npools);
      if (likely(pool->lpools[steal] != NULL))
        elm = dq_pop_bot(pool->lpools[steal]);
    }
  }
  return elm;
}

inline void* mv_pool_get_slow(mv_pool* pool) {
  void* elm = NULL;
  while (!elm) {
    int steal = rand() % (pool->npools);
    if (likely(pool->lpools[steal] != NULL))
      elm = dq_pop_bot(pool->lpools[steal]);
  }
  return elm;
}

#endif
