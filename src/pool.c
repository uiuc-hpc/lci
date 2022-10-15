#include "lcii.h"

int32_t tls_pool_struct[MAX_NPOOLS][MAX_LOCAL_POOL]; // = {-1, -1, -1, -1, -1, -1, -1, -1};
int lc_pool_nkey = 0;
LCIU_spinlock_t init_lock;
static int initialized = 0;
int LCIU_nthreads = 0;
__thread int LCIU_thread_id = -1;
__thread unsigned int LCIU_rand_seed = 0;

static inline void lc_pool_init() {
  LCIU_spinlock_init(&init_lock);
  LCIU_acquire_spinlock(&init_lock);
  if (!initialized) {
    for (int i = 0; i < MAX_NPOOLS; i++) {
      memset(&tls_pool_struct[i][0], POOL_UNINIT,
             sizeof(int32_t) * MAX_LOCAL_POOL);
    }
    initialized = 1;
  }
  LCIU_release_spinlock(&init_lock);
  LCIU_spinlock_fina(&init_lock);
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
    LCM_dq_finalize(&pool->lpools[i].dq);
    LCIU_spinlock_fina(&pool->lpools[i].lock);
  }
  LCIU_free(pool);
}

int lc_pool_count(const struct lc_pool* pool) {
  int total_num = 0;
  for (int i = 0; i < pool->npools; i++) {
    total_num += LCM_dq_size(pool->lpools[i].dq);
  }
  return total_num;
}
