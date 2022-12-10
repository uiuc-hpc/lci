#include "runtime/lcii.h"

int32_t LCII_tls_pool_metadata[MAX_NPOOLS]
                              [MAX_LOCAL_POOL];  // = {-1, -1, -1, -1, -1, -1,
                                                 // -1, -1};
int LCII_pool_nkey = 0;
LCIU_spinlock_t init_lock;
static int initialized = 0;

static inline void LCII_pool_init()
{
  LCIU_spinlock_init(&init_lock);
  LCIU_acquire_spinlock(&init_lock);
  if (!initialized) {
    for (int i = 0; i < MAX_NPOOLS; i++) {
      memset(&LCII_tls_pool_metadata[i][0], POOL_UNINIT,
             sizeof(int32_t) * MAX_LOCAL_POOL);
    }
    initialized = 1;
  }
  LCIU_release_spinlock(&init_lock);
  LCIU_spinlock_fina(&init_lock);
}

void LCII_pool_create(struct LCII_pool_t** pool)
{
  if (unlikely(!initialized)) LCII_pool_init();
  struct LCII_pool_t* p = 0;
  posix_memalign((void**)&p, LCI_CACHE_LINE, sizeof(struct LCII_pool_t));
  p->npools = 0;
  p->key = LCII_pool_nkey++;
  if (p->key < 0 || p->key > MAX_LOCAL_POOL) {
    printf("%d\n", p->key);
    printf("Unable to allocate more pool\n");
    exit(EXIT_FAILURE);
  }
  *pool = p;
}

void LCII_pool_destroy(struct LCII_pool_t* pool)
{
  for (int i = 0; i < pool->npools; i++) {
    LCM_dq_finalize(&pool->lpools[i].dq);
    LCIU_spinlock_fina(&pool->lpools[i].lock);
  }
  LCIU_free(pool);
}

int LCII_pool_count(const struct LCII_pool_t* pool)
{
  int total_num = 0;
  for (int i = 0; i < pool->npools; i++) {
    total_num += LCM_dq_size(pool->lpools[i].dq);
  }
  return total_num;
}
