#ifndef FTHREAD_H_
#define FTHREAD_H_

enum fthread_state {
  INVALID,
  DONE,
  CREATED,
  // READY, -- this may not be needed.
  YIELD,
  BLOCKED
};

struct fthread;
typedef struct fthread* fthread_t;

typedef struct fthread {
  fctx ctx;
  void* stack;
  struct fworker* origin;
  int id;
  ffunc func;
  void* data;
  volatile fthread_t* uthread;
  volatile int waiter_lock;
  struct fthread* waiter;
  volatile enum fthread_state state;
} fthread __attribute__((aligned(64)));

LC_INLINE void fthread_init(fthread* f)
{
  f->state = INVALID;
  f->stack = lc_memalign(64, 8192);
}

LC_INLINE void fthread_yield(fthread_t);
LC_INLINE void fthread_wait(fthread_t);
LC_INLINE void fthread_resume(fthread_t);
LC_INLINE void fthread_fini(fthread_t);
LC_INLINE void fthread_join(fthread_t*);

LC_INLINE void fthread_cancel(fthread* f)
{
  f->state = INVALID;
  fthread_resume(f);
}

LC_INLINE void fthread_create(fthread*, ffunc myfunc, void* data,
                              size_t stack_size);
#endif
