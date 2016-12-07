#ifndef _FULT_H_
#define _FULT_H_

#include <sys/mman.h>

#include "macro.h"
#include "bitops.h"
#include "config.h"
#include "ult.h"

#define DEBUG(x)

typedef void (*ffunc)(intptr_t);
typedef void* fcontext_t;
struct fthread;
struct fworker;

struct tls_t {
  struct fthread* thread;
  struct fworker* worker;
};

extern __thread struct tls_t tlself;

// fcontext (from boost).
fcontext_t make_fcontext(void* sp, size_t size, void (*thread_func)(intptr_t));
void* jump_fcontext(fcontext_t* old, fcontext_t, intptr_t arg);

typedef struct fctx {
  struct fctx* parent;
  fcontext_t stack_ctx;
} fctx;

MV_INLINE void swap_ctx(fctx* from, fctx* to, intptr_t args)
{
  to->parent = from;
  jump_fcontext(&(from->stack_ctx), to->stack_ctx, (intptr_t)args);
}

MV_INLINE void swap_ctx_parent(fctx* f)
{
  jump_fcontext(&(f->stack_ctx), f->parent->stack_ctx, 0);
}

static void fwrapper(intptr_t arg);

#include "fthread.h"
#include "fworker.h"

#include "fult_inl.h"

#endif
