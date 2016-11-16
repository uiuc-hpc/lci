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

class fthread final {
  friend class fworker;

 public:
  inline fthread() : state_(INVALID) { stack.sp = NULL; }
  inline ~fthread() {
    if (stack.sp != NULL) fthread_stack.deallocate(stack);
  }

  void yield();
  void wait();
  void resume();
  void fin();
  void join();
  int get_worker_id();
  void start();

  inline void cancel() {
    state_ = INVALID;
    resume();
  }

  void init(ffunc myfunc, intptr_t data, size_t stack_size);
  inline fthread_state state() { return state_; }
  inline fctx* ctx() { return &ctx_; }
  inline int id() { return id_; }
  inline fworker* origin() { return origin_; }

  std::atomic<int> count;

 private:
  fworker* origin_;
  int id_;
  volatile fthread_state state_;
  fctx ctx_;
  ffunc myfunc_;
  intptr_t data_;
  stack_context stack;
} __attribute__((aligned(64)));

#endif
