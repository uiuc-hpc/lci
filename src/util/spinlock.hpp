#ifndef LCIXX_SPINLOCK_HPP
#define LCIXX_SPINLOCK_HPP

#include <pthread.h>

namespace lcixx
{
class spinlock_t
{
 public:
  spinlock_t() { pthread_spin_init(&l, PTHREAD_PROCESS_PRIVATE); }

  ~spinlock_t() { pthread_spin_destroy(&l); }

  bool try_lock() { return pthread_spin_trylock(&l) == 0; }

  void lock() { pthread_spin_lock(&l); }

  void unlock() { pthread_spin_unlock(&l); }

 private:
  pthread_spinlock_t l;
};
}  // namespace lcixx

#endif  // LCIXX_SPINLOCK_HPP
