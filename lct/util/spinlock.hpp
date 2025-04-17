#ifndef LCT_SPINLOCK_HPP
#define LCT_SPINLOCK_HPP

#include <pthread.h>

namespace lct
{
class spinlock_t
{
 public:
#ifdef __APPLE__
  // Mac OS X doesn't support pthread_spinlock_t
  // What makes it worse is that OSSpinLock is deprecated in Mac OS
  // and in general spinlocks are not safe on Mac OS
  // https://mjtsai.com/blog/2015/12/16/osspinlock-is-unsafe/
  // so we use pthread_mutex_t instead
  spinlock_t() { pthread_mutex_init(&m_lock, nullptr); }
  ~spinlock_t() { pthread_mutex_destroy(&m_lock); }

  bool try_lock() { return pthread_mutex_trylock(&m_lock); }

  void lock() { pthread_mutex_lock(&m_lock); }

  void unlock() { pthread_mutex_unlock(&m_lock); }

 private:
  pthread_mutex_t m_lock;
#else
  spinlock_t() { pthread_spin_init(&m_lock, PTHREAD_PROCESS_PRIVATE); }

  ~spinlock_t() { pthread_spin_destroy(&m_lock); }

  bool try_lock() { return pthread_spin_trylock(&m_lock) == 0; }

  void lock() { pthread_spin_lock(&m_lock); }

  void unlock() { pthread_spin_unlock(&m_lock); }

 private:
  pthread_spinlock_t m_lock;
#endif
};
}  // namespace lct

#endif  // LCT_SPINLOCK_HPP
