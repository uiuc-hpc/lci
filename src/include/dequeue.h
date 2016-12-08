#ifndef MV_DEQUEUE_H_
#define MV_DEQUEUE_H_

#define MAX_SIZE (1 << 12)
#include "mv/lock.h"

struct dequeue {
  volatile int spinlock;
  size_t top;
  size_t bot;
  size_t size;
  void* cache;
  void* container[MAX_SIZE];
} __attribute__((aligned(64)));

MV_INLINE void dq_init(struct dequeue* dq, size_t size);
MV_INLINE void* dq_pop_top(struct dequeue* deq);
MV_INLINE void* dq_push_top(struct dequeue* deq, void* p);
MV_INLINE void* dq_pop_bot(struct dequeue* deq);

MV_INLINE void dq_init(struct dequeue* dq, size_t size)
{
  memset(dq->container, 0, MAX_SIZE);
  dq->top = 0;
  dq->bot = 0;
  dq->size = size;
  dq->cache = NULL;
  dq->spinlock = MV_SPIN_UNLOCKED;
}

MV_INLINE void* dq_pop_top(struct dequeue* deq)
{
  void* ret = NULL;
  mv_spin_lock(&deq->spinlock);
  if (deq->top != deq->bot) {
    deq->top = (deq->top + MAX_SIZE - 1) & (MAX_SIZE - 1);
    ret = deq->container[deq->top];
  }
  mv_spin_unlock(&deq->spinlock);
  return ret;
};

MV_INLINE void* dq_push_top(struct dequeue* deq, void* p)
{
  void* ret = NULL;
  mv_spin_lock(&deq->spinlock);
  deq->container[deq->top] = p;
  deq->top = (deq->top + 1) & (MAX_SIZE - 1);
  if (((deq->top + MAX_SIZE - deq->bot) & (MAX_SIZE - 1)) > deq->size) {
    ret = deq->container[deq->bot];
    deq->bot = (deq->bot + 1) & (MAX_SIZE - 1);
  }
  mv_spin_unlock(&deq->spinlock);
  return ret;
};

MV_INLINE void* dq_pop_bot(struct dequeue* deq)
{
  void* ret = NULL;
  mv_spin_lock(&deq->spinlock);
  if (deq->top != deq->bot) {
    ret = deq->container[deq->bot];
    deq->bot = (deq->bot + 1) & (MAX_SIZE - 1);
  }
  mv_spin_unlock(&deq->spinlock);
  return ret;
};



#endif
