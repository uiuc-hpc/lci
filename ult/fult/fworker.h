#ifndef F_WORKER_H_
#define F_WORKER_H_

static std::atomic<int> nfworker_;
class fworker final {
  friend class fthread;

 public:
  fworker();
  ~fworker();

  fthread* spawn(ffunc f, intptr_t data = 0, size_t stack_size = F_STACK_SIZE);
  void work(fthread* f);

  inline void start() {
    stop_ = false;
    w_ = std::thread(wfunc, this);
  }

  inline void stop() {
    stop_ = true;
    w_.join();
  }

  inline void start_main(ffunc main_task, intptr_t data = 0) {
    stop_ = false;
    spawn(main_task, data, MAIN_STACK_SIZE);
    wfunc(this);
  }

  inline void stop_main() { stop_ = true; }

  inline int id() { return id_; }

 private:
  static void wfunc(fworker*);

  fthread* fthread_new(const int id, ffunc f, intptr_t data, size_t stack_size);
  void schedule(const int id);
  void fin(int id);
  struct {
    bool stop_;
    fctx ctx_;
#ifdef USE_L1_MASK
    unsigned long l1_mask[8];
#endif
    unsigned long mask_[NMASK * WORDSIZE];
  } __attribute__((aligned(64)));

  fthread* thread_;
  std::thread w_;
  int id_;
  std::stack<fthread*> thread_pool_;
  std::atomic_flag thread_pool_lock_;
} __attribute__((aligned(64)));

#endif
