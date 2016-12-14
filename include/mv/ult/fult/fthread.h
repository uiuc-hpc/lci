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

typedef struct fthread {
  fctx ctx;
  void* stack;
  struct fworker* origin;
  int id;
  ffunc func;
  intptr_t data;
  volatile enum fthread_state state;
  volatile int count;
} fthread __attribute__((aligned(64)));

MV_INLINE void fthread_init(fthread* f)
{
  f->state = INVALID;
  f->stack = NULL;
}

MV_INLINE void fthread_destory(fthread* f)
{
  if (f->stack != NULL)
    free((void*) ((uintptr_t)f->stack - F_STACK_SIZE));
}

MV_INLINE void fthread_yield(fthread*);
MV_INLINE void fthread_wait(fthread*);
MV_INLINE void fthread_resume(fthread*);

MV_INLINE void fthread_fini(fthread*);
MV_INLINE void fthread_join(fthread*);
MV_INLINE void fthread_cancel(fthread* f)
{
  f->state = INVALID;
  fthread_resume(f);
}

MV_INLINE void fthread_create(fthread*, ffunc myfunc, intptr_t data,
                              size_t stack_size);
#endif
