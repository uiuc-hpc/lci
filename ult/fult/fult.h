#ifndef _FULT_H_
#define _FULT_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <stack>

#include "standard_stack_allocator.hpp"
#include <boost/coroutine/stack_context.hpp>

#include <sys/mman.h>

#include "config.h"
#include "affinity.h"
#include "bitops.h"
#include "profiler.h"
#include "ult.h"

using boost::coroutines::standard_stack_allocator;
using boost::coroutines::stack_context;

#define DEBUG(x)

#define SPIN_LOCK(l) while (l.test_and_set(std::memory_order_acquire));  // acquire lock
#define SPIN_UNLOCK(l) l.clear(std::memory_order_release); 

typedef void* fcontext_t;
class fthread;
class fworker;

struct tls_t {
  fthread* thread;
  fworker* worker;
};

__thread tls_t tlself;

// fcontext (from boost).
extern "C" {
fcontext_t make_fcontext(void* sp, size_t size, void (*thread_func)(intptr_t));
void* jump_fcontext(fcontext_t* old, fcontext_t, intptr_t arg);
}

struct fctx {
  inline void swap_ctx(fctx* to, intptr_t args) {
    to->parent_ = this;
    jump_fcontext(&(this->myctx_), to->myctx_, (intptr_t)args);
  }

  inline void swap_ctx_parent() {
    jump_fcontext(&(this->myctx_), parent_->myctx_, 0);
  }

  fctx* parent_;
  fcontext_t myctx_;
};

static standard_stack_allocator fthread_stack;
static void fwrapper(intptr_t);

#include "fworker.h"
#include "fthread.h"

#include "fult_inl.h"

#endif
