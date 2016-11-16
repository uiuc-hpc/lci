#ifndef PACKET_MANAGER_NUMA_STEAL_H_
#define PACKET_MANAGER_NUMA_STEAL_H_

#include "lock.h"
#include "packet.h"
#include "packet_pool.h"

int worker_id();

void ns_pp_init(mv_pp**);
void ns_pp_destroy(mv_pp*);
void ns_pp_ext(mv_pp*, int nworker);

void ns_pp_free(mv_pp*, packet*, int pid);
MV_INLINE packet* ns_pp_alloc_send(mv_pp*);
MV_INLINE packet* ns_pp_alloc_recv_nb(mv_pp*);

template<class T> class ArrPool;

template <class T>
class ArrPool {
 public:
  static const size_t MAX_SIZE = (1 << 12);
  ArrPool(size_t max_size)
      : flag(MV_SPIN_UNLOCKED),
        max_size_(max_size),
        top_(0),
        bottom_(0),
        container_(new T[MAX_SIZE]) {
    memset(container_, 0, MAX_SIZE * sizeof(T));
  }

  ~ArrPool() { delete[] container_; }

  MV_INLINE T popTop() {
    T ret = 0;
    mv_spin_lock(&flag);
    if (top_ != bottom_) {
      top_ = (top_ + MAX_SIZE - 1) & (MAX_SIZE - 1);
      ret = container_[top_];
    }
    mv_spin_unlock(&flag);
    return ret;
  };

  MV_INLINE T pushTop(T p) {
    T ret = 0;
    mv_spin_lock(&flag);
    container_[top_] = p;
    top_ = (top_ + 1) & (MAX_SIZE - 1);
    if (((top_ + MAX_SIZE - bottom_) & (MAX_SIZE - 1)) > max_size_) {
      ret = container_[bottom_];
      bottom_ = (bottom_ + 1) & (MAX_SIZE - 1);
    }
    mv_spin_unlock(&flag);
    return ret;
  };

  MV_INLINE T popBottom() {
    T ret = 0;
    mv_spin_lock(&flag);
    if (top_ != bottom_) {
      ret = container_[bottom_];
      bottom_ = (bottom_ + 1) & (MAX_SIZE - 1);
    }
    mv_spin_unlock(&flag);
    return ret;
  };

 private:
  volatile int flag;
  size_t max_size_;
  size_t top_;
  size_t bottom_;
  T* container_;
} __attribute__((aligned(64)));

struct ns_pp {
  std::vector<ArrPool<packet*>*> prv_pool;
  int nworker_;
} __attribute__((aligned(64)));

inline void ns_pp_init(mv_pp** pp_) {
  ns_pp** pp = (ns_pp**) pp_;
  *pp = new ns_pp();
  (*pp)->prv_pool.emplace_back(new ArrPool<packet*>(MAX_CONCURRENCY));
  (*pp)->nworker_ = 0;
}
#undef mv_pp_init
#define mv_pp_init ns_pp_init

inline void ns_pp_ext(mv_pp* pp_, int nworker) {
  ns_pp* pp = (ns_pp*) pp_;
  for (int i = pp->nworker_; i < nworker; i++) {
    pp->prv_pool.emplace_back(new ArrPool<packet*>(MAX_CONCURRENCY));
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

void ns_pp_free(mv_pp* mv_pp_, packet* p, const int where) ;

inline void ns_pp_free(mv_pp* mv_pp_, packet* p, const int where) {
  ns_pp* pp = (ns_pp*) mv_pp_;
  pp->prv_pool[where]->pushTop(p);
}
#undef mv_pp_free
#define mv_pp_free ns_pp_free

MV_INLINE packet* ns_pp_alloc_send(mv_pp* mv_pp_) {
  ns_pp* pp = (ns_pp*) mv_pp_;
  packet* p = pp->prv_pool[worker_id() + 1]->popTop();
  while (!p) {
    p = pp->prv_pool[worker_id() + 1]->popTop();
    for (int steal = 0; steal < pp->nworker_ + 1; steal++) {
      p = pp->prv_pool[steal]->popBottom();
      if (p) break;
    }
  }
  p->header().poolid = worker_id() + 1;
  return p;
}
#undef mv_pp_alloc_send
#define mv_pp_alloc_send ns_pp_alloc_send

MV_INLINE packet* ns_pp_alloc_recv_nb(mv_pp* mv_pp_) {
  ns_pp* pp = (ns_pp*) mv_pp_;
  packet* p = pp->prv_pool[0]->popTop();
  if (!p) {
    p = pp->prv_pool[0]->popTop();
    for (int steal = 1; steal < pp->nworker_ + 1; steal++) {
      p = pp->prv_pool[steal]->popBottom();
      if (p) break;
    }
  }
  return p;
}
#undef mv_pp_alloc_recv_nb
#define mv_pp_alloc_recv_nb ns_pp_alloc_recv_nb

#endif
