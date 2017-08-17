#ifndef _FULT_H_
#define _FULT_H_

#include <sys/mman.h>

#include "bitops.h"
#include "config.h"
#include "lc/macro.h"

#define DEBUG(x)

typedef void* (*ffunc)(void*);
typedef void* fcontext_t;
struct fthread;
struct fworker;

struct tls_t {
  struct fthread* thread;
  struct fworker* worker;
};

extern __thread struct tls_t tlself;

// fcontext (from boost).
#ifdef __cplusplus
extern "C" {
#endif
fcontext_t make_fcontext(void* sp, size_t size, void* (*thread_func)(void*));
void* jump_fcontext(fcontext_t* old, fcontext_t, void* arg);
#ifdef __cplusplus
}
#endif

typedef struct fctx {
  struct fctx* parent;
  fcontext_t stack_ctx;
} fctx;

LC_INLINE void swap_ctx(fctx* from, fctx* to, void* args)
{
  to->parent = from;
  jump_fcontext(&(from->stack_ctx), to->stack_ctx, args);
}

LC_INLINE void swap_ctx_parent(fctx* f)
{
  jump_fcontext(&(f->stack_ctx), f->parent->stack_ctx, f->parent);
}

static void* fwrapper(void* arg);

#include "fthread.h"
#include "fworker.h"

#include "fult_inl.h"

#endif
