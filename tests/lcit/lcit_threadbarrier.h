#ifndef LCIT_THREADBARRIER_HPP
#define LCIT_THREADBARRIER_HPP

class ThreadBarrier
{
 public:
  ThreadBarrier(size_t thread_num)
      : waiting(0), step(0), thread_num_(thread_num)
  {
    LCT_Assert(LCT_log_ctx_default, thread_num_ > 0,
               "Error: thread_num cannot be 0.\n");
  }

  void set_thread_num(int thread_num)
  {
    LCT_Assert(LCT_log_ctx_default, thread_num_ > 0,
               "Error: thread_num cannot be 0.\n");
  }

  void wait()
  {
    LCT_Assert(LCT_log_ctx_default, thread_num_ > 0,
               "Error: call wait() before init().\n");
    size_t mstep = step.load();

    if (++waiting == thread_num_) {
      waiting = 0;
      step++;
    } else {
      while (step == mstep) continue;
    }
  }

  template <typename Fn, typename... Args>
  void wait(Fn&& fn, Args&&... args)
  {
    LCT_Assert(LCT_log_ctx_default, thread_num_ > 0,
               "Error: call wait() before init().\n");
    size_t mstep = step.load();

    if (++waiting == thread_num_) {
      waiting = 0;
      step++;
    } else {
      while (step == mstep) fn(std::forward<Args>(args)...);
    }
  }

 private:
  alignas(64) std::atomic<size_t> waiting;
  alignas(64) std::atomic<size_t> step;
  alignas(64) size_t thread_num_;
};

#endif  // LCIT_THREADBARRIER_HPP
