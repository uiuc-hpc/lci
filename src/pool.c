#include "pool.h"
#include "ptmalloc.h"

int32_t tls_pool_struct[MAX_NPOOLS][MAX_LOCAL_POOL]; // = {-1, -1, -1, -1, -1, -1, -1, -1};
int lc_pool_nkey = 0;
volatile int init_pool_lock = 0;

void lc_pool_init() {
  for (int i = 0; i < MAX_NPOOLS; i++) {
    for (int j = 0; j < MAX_LOCAL_POOL; j++) {
      tls_pool_struct[i][j] = -1;
    }
  }
}

void lc_pool_create(struct lc_pool** pool) {
  struct lc_pool* p = memalign(64, sizeof(struct lc_pool));
  p->npools = 0;
  p->key = lc_pool_nkey++;
  if (p->key < 0 || p->key > MAX_LOCAL_POOL) {
    printf("Unable to allocate more pool\n");
    exit(EXIT_FAILURE);
  }
  *pool = p;
}

void lc_pool_destroy(struct lc_pool* pool) {
  for (int i = 0; i < pool->npools; i++) {
    free(pool->lpools[i]);
  }
  free(pool);
}

void lc_pool_put(struct lc_pool* pool, void* elm) {
  int32_t pid = lc_pool_get_local(pool);
  struct dequeue* lpool = pool->lpools[pid];
  if (lpool->cache == NULL) {
    lpool->cache = elm;
  } else {
    dq_push_top(lpool, elm);
  }
}

void lc_pool_put_to(struct lc_pool* pool, void* elm, int32_t pid) {
  struct dequeue* lpool = pool->lpools[pid];
  dq_push_top(lpool, elm);
}

void* lc_pool_get(struct lc_pool* pool) {
  int32_t pid = lc_pool_get_local(pool);
  struct dequeue* lpool = pool->lpools[pid];
  void *elm = NULL;
  if (lpool->cache != NULL) {
    elm = lpool->cache;
    lpool->cache = NULL;
  } else {
    elm = dq_pop_top(lpool);
    if (elm == NULL)
      elm = lc_pool_get_slow(pool);
  }
  return elm;
}

void* lc_pool_get_nb(struct lc_pool* pool) {
  int32_t pid = lc_pool_get_local(pool);
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

void* lc_pool_get_slow(struct lc_pool* pool) {
  void* elm = NULL;
  while (!elm) {
    int steal = rand() % (pool->npools);
    if (likely(pool->lpools[steal] != NULL))
      elm = dq_pop_bot(pool->lpools[steal]);
  }
  return elm;
}
