#ifndef MV_DEQUEUE_H_
#define MV_DEQUEUE_H_

#define MAX_SIZE (1 << 12)
#include "mv/lock.h"
#include <string.h>

struct dequeue {
  struct {
    void* cache;
    size_t top;
    size_t bot;
  } __attribute__((aligned(64)));
  volatile int spinlock;
  void* container[MAX_SIZE];
} __attribute__((aligned(64)));

MV_INLINE void dq_init(struct dequeue* dq);
MV_INLINE void dq_push_top(struct dequeue* deq, void* p);
MV_INLINE void* dq_pop_top(struct dequeue* deq);
MV_INLINE void* dq_pop_bot(struct dequeue* deq);

MV_INLINE void dq_init(struct dequeue* dq)
{
  memset(dq->container, 0, MAX_SIZE);
  dq->top = 0;
  dq->bot = 0;
  dq->cache = NULL;
  dq->spinlock = MV_SPIN_UNLOCKED;
}

MV_INLINE void* dq_pop_top(struct dequeue* deq)
{
  void* ret = NULL;
  lc_spin_lock(&deq->spinlock);
  if (deq->top != deq->bot) {
    deq->top = (deq->top + MAX_SIZE - 1) & (MAX_SIZE - 1);
    ret = deq->container[deq->top];
  }
  lc_spin_unlock(&deq->spinlock);
  return ret;
};

MV_INLINE void dq_push_top(struct dequeue* deq, void* p)
{
  lc_spin_lock(&deq->spinlock);
  deq->container[deq->top] = p;
  deq->top = (deq->top + 1) & (MAX_SIZE - 1);
  if (deq->top == deq->bot) {
    fprintf(stderr, "pool overflow\n");
    exit(EXIT_FAILURE);
  }
  lc_spin_unlock(&deq->spinlock);
};

MV_INLINE void* dq_pop_bot(struct dequeue* deq)
{
  void* ret = NULL;
  lc_spin_lock(&deq->spinlock);
  if (deq->top != deq->bot) {
    ret = deq->container[deq->bot];
    deq->bot = (deq->bot + 1) & (MAX_SIZE - 1);
  }
  lc_spin_unlock(&deq->spinlock);
  return ret;
};

#endif
