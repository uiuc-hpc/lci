#ifndef LC_DEQUEUE_H_
#define LC_DEQUEUE_H_

#define MAX_SIZE (1 << 12)
#include <string.h>
#include <stdio.h>
#include "lciu.h"

struct dequeue {
  struct {
    void* cache;
    size_t top;
    size_t bot;
  } __attribute__((aligned(64)));
  LCIU_mutex_t spinlock;
  void* container[MAX_SIZE];
} __attribute__((aligned(64)));

static inline void dq_init(struct dequeue* dq);
static inline void dq_push_top(struct dequeue* deq, void* p);
static inline void* dq_pop_top(struct dequeue* deq);
static inline void* dq_pop_bot(struct dequeue* deq);

static inline void dq_init(struct dequeue* dq)
{
  memset(dq->container, 0, sizeof(void*) * MAX_SIZE);
  dq->top = 0;
  dq->bot = 0;
  dq->cache = NULL;
  dq->spinlock = LCIU_SPIN_UNLOCKED;
}

static inline void* dq_pop_top(struct dequeue* deq)
{
  void* ret = NULL;
  LCIU_acquire_spinlock(&deq->spinlock);
  if (deq->top != deq->bot) {
    deq->top = (deq->top + MAX_SIZE - 1) & (MAX_SIZE - 1);
    ret = deq->container[deq->top];
  }
  LCIU_release_spinlock(&deq->spinlock);
  return ret;
};

static inline void dq_push_top(struct dequeue* deq, void* p)
{
  LCIU_acquire_spinlock(&deq->spinlock);
  deq->container[deq->top] = p;
  deq->top = (deq->top + 1) & (MAX_SIZE - 1);
  if (unlikely(deq->top == deq->bot)) {
    fprintf(stderr, "pool overflow\n");
    exit(EXIT_FAILURE);
  }
  LCIU_release_spinlock(&deq->spinlock);
};

static inline void* dq_pop_bot(struct dequeue* deq)
{
  void* ret = NULL;
  LCIU_acquire_spinlock(&deq->spinlock);
  if (deq->top != deq->bot) {
    ret = deq->container[deq->bot];
    deq->bot = (deq->bot + 1) & (MAX_SIZE - 1);
  }
  LCIU_release_spinlock(&deq->spinlock);
  return ret;
};

#endif
