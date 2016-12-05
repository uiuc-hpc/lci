#ifndef F_WORKER_H_
#define F_WORKER_H_

struct fworker {
  fthread* threads;
  std::thread runner;
  std::stack<fthread*> thread_pool;
  volatile int thread_pool_lock __attribute__((aligned(64)));
  int id;
  struct {
    bool stop;
    fctx ctx;
#ifdef USE_L1_MASK
    unsigned long l1_mask[8];
#endif
    unsigned long mask[NMASK * WORDSIZE];
  } __attribute__((aligned(64)));
} __attribute__((aligned(64)));

MV_INLINE fthread* fworker_spawn(fworker*, ffunc f, intptr_t data = 0,
                                 size_t stack_size = F_STACK_SIZE);
MV_INLINE void fworker_init(fworker**);
MV_INLINE void fworker_work(fthread*);
MV_INLINE void fworker_sched_thread(fworker* w, const int tid);
MV_INLINE void fworker_fini_thread(fworker* w, const int tid);

MV_INLINE static void wfunc(fworker*);

MV_INLINE void fworker_start(fworker* w)
{
  w->stop = 0;
  w->runner = std::thread(wfunc, w);
}

MV_INLINE void fworker_stop(fworker* w)
{
  w->stop = 1;
  w->runner.join();
}

MV_INLINE void fworker_start_main(fworker* w, ffunc main_task,
                                  intptr_t data = 0)
{
  w->stop = 0;
  fworker_spawn(w, main_task, data, MAIN_STACK_SIZE);
  wfunc(w);
}

MV_INLINE void fworker_stop_main(fworker* w) { w->stop = true; }
MV_INLINE int fworker_id(fworker* w) { return w->id; }

#endif
