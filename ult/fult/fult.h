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
static const int F_STACK_SIZE = 4*1024;
static const int MAIN_STACK_SIZE = 16*1024;

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
static void fwrapper(intptr_t);
class fult;
class fworker;

__thread fult* __fulting;

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

class fult final : public ult_base {
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

  void start();
  void done();

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

#ifdef USE_WORKER_WAIT
// This is for the worker to wait instead the thread.
// In that case, we do not support yielding.
  void wait(bool& flag);
  void resume(bool& flag);
#endif

 private:
  fult_t fult_new(const int id, ffunc f, intptr_t data, size_t stack_size);
  static void wfunc(fworker*);

  void schedule(const int id);
  inline void fin(int id) { tid_pool->push(&lwt_[id]); }

  fctx ctx_;
  fult lwt_[WORDSIZE * NMASK];

  volatile unsigned long l1_mask;
  volatile unsigned long mask_[NMASK];
  volatile bool stop_;

  std::thread w_;
  int id_;

#ifdef USE_WORKER_WAIT
  std::mutex m_;
  std::condition_variable cv_;
#endif

  // TODO(danghvu): this is temporary, but it is most generic.
  std::unique_ptr<boost::lockfree::stack<fult_t>>
      tid_pool;  //, boost::lockfree::capacity<WORDSIZE * NMASK>>
} __attribute__((aligned(64)));

#include "fult_inl.h"

#endif
