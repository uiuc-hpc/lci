// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_SPINLOCK_HPP
#define LCI_SPINLOCK_HPP

#include <pthread.h>

namespace lci
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
}  // namespace lci

#endif  // LCI_SPINLOCK_HPP
