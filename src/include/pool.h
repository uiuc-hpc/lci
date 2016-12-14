#ifndef MV_POOL_H_
#define MV_POOL_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

#include "mv/ult/ult.h"
#include "dequeue.h"

#define MAX_NPOOLS 128
#define MAX_LOCAL_POOL 8

extern int mv_pool_nkey;
extern int8_t tls_pool_struct[16][MAX_LOCAL_POOL];
//extern __thread int8_t tls_pool_struct[MAX_LOCAL_POOL];

typedef struct mv_pool {
  int key;
  unsigned count;
  int npools;
  void* data;
  struct dequeue* lpools[MAX_NPOOLS];
} mv_pool __attribute__((aligned(64)));

void mv_pool_create(mv_pool** pool, void* data, size_t elm_size, unsigned count);
void mv_pool_destroy(mv_pool* pool);
void mv_pool_put(mv_pool* pool, void* elm);
void mv_pool_put_to(mv_pool* pool, void* elm, int8_t pid);
void* mv_pool_get(mv_pool* pool);
void* mv_pool_get_nb(mv_pool* pool);
void* mv_pool_get_slow(mv_pool* pool);

MV_INLINE int8_t mv_pool_get_local(mv_pool* pool) {
  // int8_t pid = tls_pool_struct[pool->key]; 
  int wid = mv_worker_id();
  int8_t pid = tls_pool_struct[wid][pool->key];
  // struct dequeue* lpool = (struct dequeue*) pthread_getspecific(pool->key);
  if (unlikely(pid == -1)) {
    struct dequeue* lpool = 0;
    posix_memalign((void**) &lpool, 64, sizeof(struct dequeue));
    dq_init(lpool, pool->count);
    pid = __sync_fetch_and_add(&pool->npools, 1);
    tls_pool_struct[wid][pool->key] = pid;
    // tls_pool_struct[pool->key] = pid;
    pool->lpools[pid] = lpool;
  }
  return pid;
}

#endif
