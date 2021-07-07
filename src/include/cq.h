#ifndef LC_CQ_H
#define LC_CQ_H

struct LCII_cq_t {
  LCM_dequeue_t dequeue;
  LCIU_spinlock_t spinlock;
} __attribute__((aligned(64)));
typedef struct LCII_cq_t LCII_cq_t;

static inline void LCII_queue_push(LCI_comp_t cq, LCII_context_t *ctx) {
  LCII_cq_t* cq_ptr = cq;
  LCIU_acquire_spinlock(&cq_ptr->spinlock);
  int ret = LCM_dq_push_top(&cq_ptr->dequeue, ctx);
  LCIU_release_spinlock(&cq_ptr->spinlock);
  LCM_Assert(ret == LCM_SUCCESS, "The completion queue is full!\n");
}

#endif // LC_CQ_H
