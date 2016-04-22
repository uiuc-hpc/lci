#ifndef _FULT_H_
#define _FULT_H_

#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <condition_variable>

#include <boost/lockfree/stack.hpp>
#include <boost/coroutine/stack_context.hpp>
#include "standard_stack_allocator.hpp"

#include <sys/mman.h>
// #define USE_L1_MASK

#include "ult.h"
#include "bitops.h"
#include "rdmax.h"
#include "affinity.h"
#include "profiler.h"

using boost::coroutines::standard_stack_allocator;
using boost::coroutines::stack_context;

#define DEBUG(x)

#ifndef USE_L1_MASK
static const int NMASK = 8;
#else
static const int NMASK = 8 * 8 * 64;
#endif
static const int WORDSIZE = (8 * sizeof(long));

typedef void* fcontext_t;
class fult;
class fworker;

__thread fult* __fulting;
__thread int __wid;

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

  inline void ret() { jump_fcontext(&(this->myctx_), parent_->myctx_, 0); }

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

static standard_stack_allocator fult_stack;

static void fwrapper(intptr_t);

class fult final {
 friend class fworker;
 public:
  inline fult() : state_(INVALID) { stack.sp = NULL; }
  inline ~fult() { if (stack.sp != NULL) fult_stack.deallocate(stack); }

  void yield();
  void wait(bool&);
  void resume(bool&);
  void join();
  int get_worker_id();

  void init(ffunc myfunc, intptr_t data, size_t stack_size);
  inline fult_state state() { return state_; }
  inline fctx* ctx() { return &ctx_; }
  inline int id() { return id_; }
  inline fworker* origin() { return origin_; }
  inline void start();

 private:
  fworker* origin_;
  int id_;
  volatile fult_state state_;
  fctx ctx_;
  ffunc myfunc_;
  intptr_t data_;
  stack_context stack;
} __attribute__((aligned(64)));

typedef fult* fult_t;

static std::atomic<int> nfworker_;

class fworker final {
 friend class fult;
 public:

   fworker();
   ~fworker();

   fult_t spawn(ffunc f, intptr_t data = 0, size_t stack_size = F_STACK_SIZE);
   void work(fult* f);

  inline void start() {
    stop_ = false;
    w_ = std::thread(wfunc, this);
  }

  inline void stop() {
    stop_ = true;
    w_.join();
  }

  inline void start_main(ffunc main_task, intptr_t data) {
    stop_ = false;
    spawn(main_task, data, MAIN_STACK_SIZE);
    wfunc(this);
  }

  inline void stop_main() {
    stop_ = true;
  }

  inline int id() { return id_; }

 private:
  fult_t fult_new(const int id, ffunc f, intptr_t data, size_t stack_size);
  static void wfunc(fworker*);
  void schedule(const int id);
  inline void fin(int id) { tid_pool->push(&lwt_[id]); }

  struct {
    bool stop_;
    fctx ctx_;
#ifdef USE_L1_MASK
    unsigned long l1_mask[8];
#endif
    unsigned long mask_[NMASK * WORDSIZE];
  } __attribute__((aligned(64)));

  fult* lwt_;
  std::thread w_;
  int id_;
  std::unique_ptr<boost::lockfree::stack<fult_t>> tid_pool;
} __attribute__((aligned(64)));

#include "fult_inl.h"

#endif
