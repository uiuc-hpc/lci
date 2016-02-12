#ifndef _FULT_H_
#define _FULT_H_

#include <atomic>
#include <thread>
#include <boost/lockfree/stack.hpp>
#include <boost/coroutine/stack_allocator.hpp>
#include <boost/coroutine/stack_context.hpp>

#include <sys/mman.h>

#include "bitops.h"
#include "rdmax.h"
#include "affinity.h"
#include "profiler.h"

using boost::coroutines::stack_allocator;
using boost::coroutines::stack_context;

#define DEBUG(x)

#define xlikely(x) __builtin_expect((x), 1)
#define xunlikely(x) __builtin_expect((x), 0)

#define fult_yield()           \
  {                            \
    if (__fulting != NULL)      \
      __fulting->yield(); \
    else                       \
      sched_yield();           \
  }

#define fult_wait() \
  { __fulting->wait(); }

#define F_STACK_SIZE (4096)
#define MAIN_STACK_SIZE (16*1024)
#define NMASK 2
#define WORDSIZE (8 * sizeof(long))

#define MEMFENCE asm volatile("" : : : "memory")

typedef void (*ffunc)(intptr_t);
static void fwrapper(intptr_t);

class fult;
class worker;
__thread fult* __fulting = NULL;

typedef void* fcontext_t;

extern "C" {
fcontext_t make_fcontext(void* sp, size_t size, void (*thread_func)(intptr_t));
void* jump_fcontext(fcontext_t* old, fcontext_t, intptr_t arg);
}

struct fctx {
  inline void swap(fctx* to, intptr_t args) {
    to->parent_ = this;
    jump_fcontext(&(this->myctx_), to->myctx_, (intptr_t)args);
  }

  inline void ret() {
    jump_fcontext(&(this->myctx_), parent_->myctx_, 0);
  }

  fctx* parent_;
  fcontext_t myctx_;
};

enum fult_state {
  INVALID,
  CREATED,
  // READY, -- this may not be needed.
  YIELD,
  BLOCKED
};

static stack_allocator fult_stack;

class fult {
 public:
  fult() : state_(INVALID), stack(NULL) {}
  ~fult() {
    if (stack != NULL) { 
      fult_stack.deallocate(*stack);
      free(stack);
    }
  }
  inline void init(worker* origin, int id) {
    origin_ = origin;
    id_ = id;
    stack = NULL;
  }

  inline void set(ffunc myfunc, intptr_t data, size_t stack_size);
  inline void yield();
  inline void wait();
  inline void resume();

  inline void start();
  inline fult_state state() { return state_; }
  inline bool is_done() { return state_ == INVALID; }
  inline fctx* ctx() { return &ctx_; }

 private:
  worker* origin_;
  int id_;
  volatile fult_state state_;
  fctx ctx_;

  ffunc myfunc_;
  intptr_t data_;
  stack_context* stack;

} __attribute__((aligned(64)));

typedef fult* fult_t;

class worker {
 public:
  inline worker() : stop_(true) {};

  inline void init() {
    stop_ = false;
    // Reset all mask.
    for (int i = 0; i < NMASK; i++) mask_[i] = 0;
    // Add all free slot.
    memset(lwt_, 0, sizeof(fult) * (NMASK * WORDSIZE));

    for (int i = (int)(NMASK * WORDSIZE)-1; i>=0; i--) { 
      tid_poll.push(i);
      lwt_[i].init(this, i);
    }
  }

  inline fult_t spawn(
    ffunc f, intptr_t data, size_t stack_size = F_STACK_SIZE);
  inline fult_t spawn_to(
    int tid, ffunc f, intptr_t data, size_t stack_size = F_STACK_SIZE);

  inline void join(fult_t f);
  inline void fin(int id) { tid_poll.push(id); }
  inline void schedule(const int id);

  inline void start() { w_ = std::thread(wfunc, this); while (stop_) {}; }
  inline void stop() { stop_ = true; w_.join(); }

  inline void start_main(ffunc main_task, intptr_t data) {
    init();
    spawn(main_task, data, MAIN_STACK_SIZE);
    wfunc(this);
  }
  inline void stop_main() { stop_ = true; }

  inline void work(fult* f) {
    __fulting = f;
    ctx_.swap(f->ctx(), (intptr_t) f);
    __fulting = NULL;
  }

  static void wfunc(worker*);

 private:
  inline fult_t fult_new(const int id, ffunc f, intptr_t data, size_t stack_size);
  
  fctx ctx_;
  fult lwt_[WORDSIZE * NMASK];

  volatile unsigned long mask_[NMASK];
  volatile bool stop_;

  std::thread w_;

  // TODO(danghvu): this is temporary, but it is most generic.
  boost::lockfree::stack<uint8_t, boost::lockfree::capacity<WORDSIZE * NMASK>>
      tid_poll;
} __attribute__((aligned(64)));


void fult::set(ffunc myfunc, intptr_t data, size_t stack_size) {
  if (stack == NULL) {
    stack = new stack_context();
    fult_stack.allocate(*stack, stack_size);
  }
  myfunc_ = myfunc;
  data_ = data;
  state_ = CREATED;
  ctx_.myctx_ = make_fcontext(stack->sp, stack->size, fwrapper);
}

void fult::yield() {
  state_ = YIELD;
  ctx_.ret();
}

void fult::wait() {
  state_ = BLOCKED;
  ctx_.ret();
}

void fult::resume() {
  origin_->schedule(id_);
}

void fult::start() {
  (*this->myfunc_)(this->data_);
  // when finish, needs to swap back to the parent.
  this->state_ = INVALID;
  origin_->fin(id_);
  ctx_.ret();
}

static void fwrapper(intptr_t args) {
  fult* ff = (fult*) args;
  ff->start();
}

fult_t worker::spawn(ffunc f, intptr_t data, size_t stack_size) {
  int id = -1;
  while (!tid_poll.pop(id)) {
    fult_yield();
  }
  return fult_new(id, f, data, stack_size);
}

fult_t worker::spawn_to(int tid, ffunc f, intptr_t data, size_t stack_size) {
  int id = -1;
  while (tid_poll.pop(id)) {
    if (id == tid) break;
    tid_poll.push(id);
  }
  return fult_new(id, f, data, stack_size);
}

void worker::join(fult* f) {
  while (!f->is_done()) {
    fult_yield();
  }
}

void worker::schedule(const int id) {
  sync_set_bit(id & (WORDSIZE - 1), &mask_[id >> 6]);
}

fult_t worker::fult_new(const int id, ffunc f, intptr_t data, size_t stack_size) {
  // add it to the fult.
  lwt_[id].set(f, data, stack_size);

  // make it schedable.
  schedule(id);

  return (fult_t) &lwt_[id];
}

#define doschedule(i)                                              \
  {                                                                \
    fult* f = &w->lwt_[(i << 6) + id]; \
    if (f->state() != INVALID) w->work(f); \
    if (f->state() == YIELD) f->resume();  \
  }

#define loop_sched_all(mask, i)                                         \
  {                                                                     \
    register unsigned long local = exchange((unsigned long)0, &(mask)); \
    while (local > 0) {                                                 \
      int id = find_first_set(local);                                   \
      local ^= ((unsigned long)1 << id);                                \
      doschedule(i);                                                    \
    }                                                                   \
  }

std::atomic<int> fult_nworker;

void worker::wfunc(worker* w) {
#ifdef USE_AFFI
  affinity::set_me_to(fult_nworker++);
#endif

  if (w->stop_) w->init();

#ifdef USE_PAPI
  profiler wp = {PAPI_L1_DCM};
  wp.start();
#endif

  while (xunlikely(!w->stop_)) {
    for (auto i=0; i<NMASK; i++) {
      auto& mask = w->mask_[i];
      if (mask > 0) {
        loop_sched_all(mask, i);
      }
    }
  }

#ifdef USE_PAPI
  wp.stop();
  wp.print();
#endif
}

#endif
