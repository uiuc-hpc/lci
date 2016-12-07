#ifndef F_WORKER_H_
#define F_WORKER_H_

typedef struct fworker {
  fthread* threads;
  fthread** thread_pool;
  int thread_pool_last;
  pthread_t runner;
  volatile int thread_pool_lock __attribute__((aligned(64)));
  int id;
  struct {
    int stop;
    fctx ctx;
#ifdef USE_L1_MASK
    unsigned long l1_mask[8];
#endif
    unsigned long mask[NMASK * WORDSIZE];
  } __attribute__((aligned(64)));
} fworker __attribute__((aligned(64)));

MV_INLINE fthread* fworker_spawn(fworker*, ffunc f, intptr_t data,
                                 size_t stack_size);
MV_INLINE void fworker_init(fworker**);
MV_INLINE void fworker_work(fworker*, fthread*);
MV_INLINE void fworker_sched_thread(fworker* w, const int tid);
MV_INLINE void fworker_fini_thread(fworker* w, const int tid);

MV_INLINE void* wfunc(void*);

MV_INLINE void fworker_start(fworker* w)
{
  w->stop = 0;
  pthread_create(&w->runner, 0, wfunc, w);
}

MV_INLINE void fworker_stop(fworker* w)
{
  w->stop = 1;
  pthread_join(w->runner, 0);
}

MV_INLINE void fworker_start_main(fworker* w, ffunc main_task,
                                  intptr_t data)
{
  w->stop = 0;
  fworker_spawn(w, main_task, data, MAIN_STACK_SIZE);
  wfunc(w);
}

MV_INLINE void fworker_stop_main(fworker* w) { w->stop = 1; }
MV_INLINE int fworker_id(fworker* w) { return w->id; }

#endif
