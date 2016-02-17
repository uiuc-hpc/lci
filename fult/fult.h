#ifndef _FULT_H_
#define _FULT_H_

#include <atomic>
#include <thread>
#include <boost/lockfree/stack.hpp>
#include <boost/coroutine/stack_allocator.hpp>
#include <boost/coroutine/stack_context.hpp>
#include "segmented_stack.h"

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

static const int F_STACK_SIZE = 4096;
static const int MAIN_STACK_SIZE = 16 * 1024;
static const int NMASK = 1;
static const int WORDSIZE = (8 * sizeof(long));

typedef void (*ffunc)(intptr_t);
typedef void* fcontext_t;

static void fwrapper(intptr_t);

class fult;
class worker;

// local thread storage.
__thread fult* __fulting = NULL;

// fcontext (from boost).
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

class alignas(64) fult {
 public:
  fult() : state_(INVALID)  { stack.sp = NULL;}
  ~fult() { if (stack.sp != NULL) fult_stack.deallocate(stack); }

  inline void init(worker* origin, int id) {
    origin_ = origin;
    id_ = id;
  }

  inline void set(ffunc myfunc, intptr_t data, size_t stack_size);
  inline void yield();
  inline void wait();
  inline void resume();
  inline void done();

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
  stack_context stack;

};

typedef fult* fult_t;

class alignas(64) worker {
 public:
  inline worker() {
    stop_ = true;
    // Reset all mask.
    for (int i = 0; i < NMASK; i++) mask_[i] = 0;
    // Add all free slot.
    memset(lwt_, 0, sizeof(fult) * (NMASK * WORDSIZE));
    tid_pool = new boost::lockfree::stack<uint32_t>(NMASK*WORDSIZE);

    for (int i = (int)(NMASK * WORDSIZE)-1; i>=0; i--) { 
      tid_pool->push(i);
      lwt_[i].init(this, i);
    }
  }

  inline fult_t spawn(
    ffunc f, intptr_t data, size_t stack_size = F_STACK_SIZE);
  inline fult_t spawn_to(
    int tid, ffunc f, intptr_t data, size_t stack_size = F_STACK_SIZE);

  inline void join(fult_t f);
  inline void fin(int id) { tid_pool->push(id); }
  inline void schedule(const int id);

  inline void start() { stop_ = false; w_ = std::thread(wfunc, this); }
  inline void stop() { stop_ = true; w_.join(); }

  inline void start_main(ffunc main_task, intptr_t data) {
    stop_ = false;
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
  boost::lockfree::stack<uint32_t>* tid_pool; //, boost::lockfree::capacity<WORDSIZE * NMASK>>
};


void fult::set(ffunc myfunc, intptr_t data, size_t stack_size) {
  if (stack.sp == NULL) 
    fult_stack.allocate(stack, stack_size);
  myfunc_ = myfunc;
  data_ = data;
  ctx_.myctx_ = make_fcontext(stack.sp, stack.size, fwrapper);
  state_ = CREATED;
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

void fult::done() {
  origin_->fin(id_);
}

void fult::start() {
  (*myfunc_)(data_);
  // when finish, needs to swap back to the parent.
  state_ = INVALID;
  ctx_.ret();
}

static void fwrapper(intptr_t args) {
  fult* ff = (fult*) args;
  ff->start();
}

fult_t worker::spawn(ffunc f, intptr_t data, size_t stack_size) {
  int id = -1; 
  if (!tid_pool->pop(id)) {
    throw std::runtime_error("Too many threads are spawn");
  }
  return fult_new(id, f, data, stack_size);
}

fult_t worker::spawn_to(int tid, ffunc f, intptr_t data, size_t stack_size) {
  int id = -1;
  while (tid_pool->pop(id)) {
    if (id == tid) break;
    tid_pool->push(id);
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

std::atomic<int> fult_nworker;

void worker::wfunc(worker* w) {
#ifdef USE_AFFI
  affinity::set_me_to(fult_nworker++);
#endif

#ifdef USE_PAPI
  profiler wp = {PAPI_L1_DCM};
  wp.start();
#endif

  while (xunlikely(!w->stop_)) {
    for (auto i=0; i<NMASK; i++) {
      auto& mask = w->mask_[i];
      if (mask > 0) {
        // Atomic exchange to get the current waiting threads.
        auto local_mask = exchange((unsigned long)0, &(mask)); 

        // Works until it no thread is pending.
        while (local_mask > 0) {
          auto id = find_first_set(local_mask);
          // Flips the set bit.
          local_mask ^= ((unsigned long)1 << id);
          // Optains the associate thread.
          fult* f = &w->lwt_[(i << 6) + id];
          // Works on it.
          w->work(f); 
          // If after working this threads yield, set it schedulable.
          auto state = f->state();
          if (state == YIELD) f->resume();
          else if (state == INVALID) f->done();
        }
      }
    }
  }

#ifdef USE_PAPI
  wp.stop();
  wp.print();
#endif
}

#endif
