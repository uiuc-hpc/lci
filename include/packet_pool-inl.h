#ifndef PACKET_MANAGER_NUMA_STEAL_H_
#define PACKET_MANAGER_NUMA_STEAL_H_

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "lock.h"
#include "macro.h"

#define MAX_SIZE (1 << 12)

struct dequeue {
  volatile int flag;
  size_t top;
  size_t bot;
  size_t size;
  void* container[MAX_SIZE];
} __attribute__((aligned(64)));

inline void dq_init(struct dequeue* dq, size_t size) {
  memset(dq->container, 0, MAX_SIZE);
  dq->top = 0;
  dq->bot = 0;
  dq->size = size;
  dq->flag = MV_SPIN_UNLOCKED;
}

inline void* dq_pop_top(struct dequeue* deq) {
  void* ret = NULL;
  mv_spin_lock(&deq->flag);
  if (deq->top != deq->bot) {
    deq->top = (deq->top + MAX_SIZE - 1) & (MAX_SIZE - 1);
    ret = deq->container[deq->top];
  }
  mv_spin_unlock(&deq->flag);
  return ret;
};

inline void* dq_push_top(struct dequeue* deq, void* p) {
  void* ret = NULL;
  mv_spin_lock(&deq->flag);
  deq->container[deq->top] = p;
  deq->top = (deq->top + 1) & (MAX_SIZE - 1);
  if (((deq->top + MAX_SIZE - deq->bot) & (MAX_SIZE - 1)) > deq->size) {
    ret = deq->container[deq->bot];
    deq->bot = (deq->bot + 1) & (MAX_SIZE - 1);
  }
  mv_spin_unlock(&deq->flag);
  return ret;
};

inline void* dq_pop_bot(struct dequeue* deq) {
  void* ret = NULL;
  mv_spin_lock(&deq->flag);
  if (deq->top != deq->bot) {
    ret = deq->container[deq->bot];
    deq->bot = (deq->bot + 1) & (MAX_SIZE - 1);
  }
  mv_spin_unlock(&deq->flag);
  return ret;
};

#define MAX_NPOOLS 128

struct mv_pp {
  int nworker_;
  struct dequeue* prv_pool[MAX_NPOOLS];
} __attribute__((aligned(64)));

inline void mv_pp_init(mv_pp** pp_) {
  struct mv_pp** pp = (struct mv_pp**)pp_;
  posix_memalign((void**)pp, 64, sizeof(struct mv_pp));
  // (*pp) = (struct mv_pp*) malloc(sizeof (struct mv_pp));
  struct dequeue* dq;
  posix_memalign((void**)&dq, 64, sizeof(struct dequeue));
  dq_init(dq, MAX_CONCURRENCY);
  (*pp)->nworker_ = 0;
  (*pp)->prv_pool[0] = dq;
}

inline void mv_pp_ext(mv_pp* pp_, int nworker) {
  struct mv_pp* pp = (struct mv_pp*)pp_;
  for (int i = pp->nworker_; i < nworker; i++) {
    struct dequeue* dq;
    posix_memalign((void**)&dq, 64, sizeof(struct dequeue));
    pp->prv_pool[i + 1] = dq;
    dq_init(dq, MAX_CONCURRENCY);
  }
  pp->nworker_ = nworker;
}

inline void mv_pp_destroy(mv_pp* mv_pp_) {
  struct mv_pp* pp = (struct mv_pp*)mv_pp_;
  for (int i = 0; i < 1 + pp->nworker_; i++) {
    free(pp->prv_pool[i]);
  }
}

void mv_pp_free(mv_pp* mv_pp_, struct packet* p) {
  struct mv_pp* pp = (struct mv_pp*)mv_pp_;
  dq_push_top(pp->prv_pool[0], p);
}

void mv_pp_free_to(mv_pp* mv_pp_, struct packet* p, int where) {
  struct mv_pp* pp = (struct mv_pp*)mv_pp_;
  dq_push_top(pp->prv_pool[where], p);
}

struct packet* mv_pp_alloc(mv_pp* mv_pp_, int pid) {
  struct mv_pp* pp = (struct mv_pp*)mv_pp_;
  struct packet* p = (struct packet*)dq_pop_top(pp->prv_pool[pid]);
  while (!p) {
    int steal = rand() % (pp->nworker_ + 1);
    p = (struct packet*)dq_pop_bot(pp->prv_pool[steal]);
  }
  return p;
}

struct packet* mv_pp_alloc_nb(mv_pp* mv_pp_, int pid) {
  struct mv_pp* pp = (struct mv_pp*)mv_pp_;
  struct packet* p = (struct packet*)dq_pop_top(pp->prv_pool[pid]);
  if (!p) {
    int steal = rand() % (pp->nworker_ + 1);
    p = (struct packet*)dq_pop_bot(pp->prv_pool[steal]);
  }
  return p;
}

#endif
