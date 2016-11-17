#ifndef PACKET_MANAGER_NUMA_STEAL_H_
#define PACKET_MANAGER_NUMA_STEAL_H_

#include "lock.h"
#include "packet.h"
#include "packet_pool.h"

int worker_id();

void ns_pp_init(mv_pp**);
void ns_pp_destroy(mv_pp*);
void ns_pp_ext(mv_pp*, int nworker);

MV_INLINE void ns_pp_free(mv_pp*, packet*, int pid);
MV_INLINE packet* ns_pp_alloc_send(mv_pp*);
MV_INLINE packet* ns_pp_alloc_recv_nb(mv_pp*);

#define MAX_SIZE (1<<12)
struct dequeue {
  dequeue(size_t max_size)
      : flag(MV_SPIN_UNLOCKED),
        max_size_(max_size),
        top(0),
        bot(0) {
    container = (void**) malloc(MAX_SIZE);
    memset(container, 0, MAX_SIZE);
  }
  ~dequeue() { free(container); }
  volatile int flag __attribute__((aligned(64)));
  size_t max_size_;
  size_t top;
  size_t bot;
  void** container;
} __attribute__((aligned(64)));

MV_INLINE void* dq_pop_top(dequeue* deq) {
  void* ret = NULL;
  mv_spin_lock(&deq->flag);
  if (deq->top != deq->bot) {
    deq->top = (deq->top + MAX_SIZE - 1) & (MAX_SIZE - 1);
    ret = deq->container[deq->top];
  }
  mv_spin_unlock(&deq->flag);
  return ret;
};

MV_INLINE void* dq_push_top(dequeue* deq, void* p) {
  void* ret = NULL;
  mv_spin_lock(&deq->flag);
  deq->container[deq->top] = p;
  deq->top = (deq->top + 1) & (MAX_SIZE - 1);
  if (((deq->top + MAX_SIZE - deq->bot) & (MAX_SIZE - 1)) > deq->max_size_) {
    ret = deq->container[deq->bot];
    deq->bot = (deq->bot + 1) & (MAX_SIZE - 1);
  }
  mv_spin_unlock(&deq->flag);
  return ret;
};

MV_INLINE void* dq_pop_bot(dequeue* deq) {
  void* ret = NULL;
  mv_spin_lock(&deq->flag);
  if (deq->top != deq->bot) {
    ret = deq->container[deq->bot];
    deq->bot = (deq->bot + 1) & (MAX_SIZE - 1);
  }
  mv_spin_unlock(&deq->flag);
  return ret;
};

struct ns_pp {
  std::vector<dequeue*> prv_pool;
  int nworker_;
} __attribute__((aligned(64)));

inline void ns_pp_init(mv_pp** pp_) {
  ns_pp** pp = (ns_pp**) pp_;
  *pp = new ns_pp();
  (*pp)->prv_pool.emplace_back(new dequeue(MAX_CONCURRENCY));
  (*pp)->nworker_ = 0;
}
#undef mv_pp_init
#define mv_pp_init ns_pp_init

inline void ns_pp_ext(mv_pp* pp_, int nworker) {
  ns_pp* pp = (ns_pp*) pp_;
  for (int i = pp->nworker_; i < nworker; i++) {
    pp->prv_pool.emplace_back(new dequeue(MAX_CONCURRENCY));
  }
  pp->nworker_ = nworker;
}
#undef mv_pp_ext
#define mv_pp_ext ns_pp_ext

inline void ns_pp_destroy(mv_pp* mv_pp_) {
  ns_pp* pp = (ns_pp*) mv_pp_;
  for (auto& a : pp->prv_pool) delete a;
  delete pp;
}
#undef mv_pp_destroy
#define mv_pp_destroy ns_pp_destroy

MV_INLINE void ns_pp_free(mv_pp* mv_pp_, packet* p, int where = 0) {
  ns_pp* pp = (ns_pp*) mv_pp_;
  dq_push_top(pp->prv_pool[where], p);
}
#undef mv_pp_free
#define mv_pp_free ns_pp_free

MV_INLINE packet* ns_pp_alloc_send(mv_pp* mv_pp_) {
  ns_pp* pp = (ns_pp*) mv_pp_;
  const int pid = worker_id() + 1;
  packet* p = (packet*) dq_pop_top(pp->prv_pool[pid]);
  while (!p) {
    p = (packet*) dq_pop_top(pp->prv_pool[pid]);
    for (int steal = 0; steal < pp->nworker_ + 1; steal++) {
      p = (packet*) dq_pop_bot(pp->prv_pool[steal]);
      if (p) break;
    }
  }
  p->header().poolid = pid;
  return p;
}
#undef mv_pp_alloc_send
#define mv_pp_alloc_send ns_pp_alloc_send

MV_INLINE packet* ns_pp_alloc_recv_nb(mv_pp* mv_pp_) {
  ns_pp* pp = (ns_pp*) mv_pp_;
  packet* p = (packet*) dq_pop_top(pp->prv_pool[0]);
  if (!p) {
    p = (packet*) dq_pop_top(pp->prv_pool[0]);
    for (int steal = 1; steal < pp->nworker_ + 1; steal++) {
      p = (packet*) dq_pop_bot(pp->prv_pool[steal]);
      if (p) break;
    }
  }
  return p;
}
#undef mv_pp_alloc_recv_nb
#define mv_pp_alloc_recv_nb ns_pp_alloc_recv_nb

#endif
