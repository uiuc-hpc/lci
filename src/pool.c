#include "pool.h"

int32_t tls_pool_struct[MAX_NPOOLS][MAX_LOCAL_POOL]; // = {-1, -1, -1, -1, -1, -1, -1, -1};
int lc_pool_nkey = 0;
volatile int init_lock = 0;
static int initialized = 0;

static inline void lc_pool_init() {
  LCIU_acquire_spinlock(&init_lock);
  if (!initialized) {
    for (int i = 0; i < MAX_NPOOLS; i++) {
      memset(&tls_pool_struct[i][0], POOL_UNINIT,
             sizeof(int32_t) * MAX_LOCAL_POOL);
    }
    initialized = 1;
  }
  LCIU_release_spinlock(&init_lock);
}

void lc_pool_create(struct lc_pool** pool) {
  if (unlikely(!initialized))
    lc_pool_init();
  struct lc_pool* p = 0;
  posix_memalign((void**) &p, 64, sizeof(struct lc_pool));
  p->npools = 0;
  p->key = lc_pool_nkey++;
  if (p->key < 0 || p->key > MAX_LOCAL_POOL) {
    printf("%d\n", p->key);
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
