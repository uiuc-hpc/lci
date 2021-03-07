#ifndef LC_CQ_H
#define LC_CQ_H

#include "packet.h"
#include "lciu.h"
#include <string.h>
#include <stdio.h>

#define CQ_MAX_SIZE (LC_SERVER_NUM_PKTS * 2)

struct lc_cq {
  struct {
    size_t top;
    size_t bot;
  } __attribute__((aligned(64)));
  LCIU_mutex_t spinlock;
  void* container[CQ_MAX_SIZE];
} __attribute__((aligned(64)));

static inline void lc_cq_create(struct lc_cq** cq_ptr)
{
  posix_memalign((void**)cq_ptr, 64, sizeof(struct lc_cq));
  lc_cq* cq = *cq_ptr;
  memset(cq->container, 0, sizeof(void*) * CQ_MAX_SIZE);
  cq->top = 0;
  cq->bot = 0;
  cq->spinlock = LCIU_SPIN_UNLOCKED;
}

static inline void lc_cq_free(struct lc_cq* cq) { free(cq); }

static inline void lc_cq_push(struct lc_cq* cq, void* req)
{
  LCIU_acquire_spinlock(&cq->spinlock);
  cq->container[cq->top] = req;
  cq->top = (cq->top + 1) & (CQ_MAX_SIZE - 1);
  if (unlikely(cq->top == cq->bot)) {
    fprintf(stderr, "pool overflow\n");
    exit(EXIT_FAILURE);
  }
  LCIU_release_spinlock(&cq->spinlock);
};

static inline void* lc_cq_pop(struct lc_cq* cq)
{
  void* ret = NULL;
  LCIU_acquire_spinlock(&cq->spinlock);
  if (cq->top != cq->bot) {
    ret = cq->container[cq->bot];
    cq->bot = (cq->bot + 1) & (CQ_MAX_SIZE - 1);
  }
  LCIU_release_spinlock(&cq->spinlock);
  return ret;
};

#define LCI_CQ_s lc_cq

#endif
