#include <atomic>
#include "lcti.hpp"

namespace lct
{
class ThreadBarrier
{
 public:
  explicit ThreadBarrier(int thread_num)
      : waiting(0), step(0), thread_num_(thread_num)
  {
    LCT_Assert(LCT_log_ctx_default, thread_num_ > 0,
               "Error: thread_num cannot be 0.\n");
  }

  int64_t arrive()
  {
    int64_t mstep = step.load();
    if (++waiting == thread_num_) {
      waiting = 0;
      step++;
    }
    return mstep;
  }

  bool test(int64_t ticket) { return ticket != step; }

  void wait(int64_t ticket)
  {
    while (!test(ticket)) continue;
  }

  template <typename Fn, typename... Args>
  void wait(int64_t ticket, Fn&& fn, Args&&... args)
  {
    while (!test(ticket)) fn(std::forward<Args>(args)...);
  }

  void arrive_and_wait()
  {
    int64_t ticket = arrive();
    wait(ticket);
  }

 private:
  alignas(64) std::atomic<int> waiting;
  alignas(64) std::atomic<int64_t> step;
  alignas(64) int thread_num_;
};

}  // namespace lct

LCT_tbarrier_t LCT_tbarrier_alloc(int nthreads)
{
  return new lct::ThreadBarrier(nthreads);
}
void LCT_tbarrier_free(LCT_tbarrier_t* tbarrier_p)
{
  auto* p = static_cast<lct::ThreadBarrier*>(*tbarrier_p);
  delete p;
  *tbarrier_p = nullptr;
}
int64_t LCT_tbarrier_arrive(LCT_tbarrier_t tbarrier)
{
  auto* p = static_cast<lct::ThreadBarrier*>(tbarrier);
  return p->arrive();
}
bool LCT_tbarrier_test(LCT_tbarrier_t tbarrier, int64_t ticket)
{
  auto* p = static_cast<lct::ThreadBarrier*>(tbarrier);
  return p->test(ticket);
}
void LCT_tbarrier_wait(LCT_tbarrier_t tbarrier, int64_t ticket)
{
  auto* p = static_cast<lct::ThreadBarrier*>(tbarrier);
  p->wait(ticket);
}
void LCT_tbarrier_arrive_and_wait(LCT_tbarrier_t tbarrier)
{
  auto* p = static_cast<lct::ThreadBarrier*>(tbarrier);
  p->arrive_and_wait();
}