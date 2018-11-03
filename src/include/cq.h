#ifndef LC_CQ_H
#define LC_CQ_H

#include "packet.h"
#include "lc/lock.h"
#include <string.h>
#include <stdio.h>

#define CQ_MAX_SIZE (LC_SERVER_NUM_PKTS * 2)

// CQ is just a cqueue with a check for completion.
struct comp_q {
  struct {
    size_t top;
    size_t bot;
  } __attribute__((aligned(64)));
  volatile int spinlock;
  struct lc_req* container[CQ_MAX_SIZE];
} __attribute__((aligned(64)));

LC_INLINE void cq_init(struct comp_q* cq)
{
  memset(cq->container, 0, sizeof(void*) * CQ_MAX_SIZE);
  cq->top = 0;
  cq->bot = 0;
  cq->spinlock = LC_SPIN_UNLOCKED;
}

LC_INLINE void cq_push(struct comp_q* cq, lc_req* req)
{
  lc_spin_lock(&cq->spinlock);
  cq->container[cq->top] = req;
  cq->top = (cq->top + 1) & (CQ_MAX_SIZE - 1);
  if (unlikely(cq->top == cq->bot)) {
    fprintf(stderr, "pool overflow\n");
    exit(EXIT_FAILURE);
  }
  lc_spin_unlock(&cq->spinlock);
};

LC_INLINE void* cq_pop(struct comp_q* cq)
{
  lc_req* ret = NULL;
  lc_spin_lock(&cq->spinlock);
  if (cq->top != cq->bot) {
    ret = cq->container[cq->bot];
    cq->bot = (cq->bot + 1) & (CQ_MAX_SIZE - 1);
  }
  lc_spin_unlock(&cq->spinlock);
  return ret;
};

#endif
