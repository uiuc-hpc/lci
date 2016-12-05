#ifndef _FULT_H_
#define _FULT_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stack>
#include <thread>

#include "standard_stack_allocator.hpp"

#include <sys/mman.h>

#include "affinity.h"
#include "bitops.h"
#include "config.h"
#include "profiler.h"
#include "ult.h"

using boost::coroutines::standard_stack_allocator;

#define DEBUG(x)

typedef void (*ffunc)(intptr_t);
typedef void* fcontext_t;
struct fthread;
struct fworker;

struct tls_t {
  fthread* thread;
  fworker* worker;
};

extern __thread tls_t tlself;

// fcontext (from boost).
extern "C" {
fcontext_t make_fcontext(void* sp, size_t size, void (*thread_func)(intptr_t));
void* jump_fcontext(fcontext_t* old, fcontext_t, intptr_t arg);
}

struct fctx {
  fctx* parent;
  fcontext_t stack_ctx;
};

MV_INLINE static void swap_ctx(fctx* from, fctx* to, intptr_t args)
{
  to->parent = from;
  jump_fcontext(&(from->stack_ctx), to->stack_ctx, (intptr_t)args);
}

MV_INLINE static void swap_ctx_parent(fctx* f)
{
  jump_fcontext(&(f->stack_ctx), f->parent->stack_ctx, 0);
}

static standard_stack_allocator fthread_stack;
static void fwrapper(intptr_t);

#include "fthread.h"
#include "fworker.h"

#include "fult_inl.h"

#endif
