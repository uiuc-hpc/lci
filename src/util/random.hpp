// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_UTIL_RANDOM_HPP
#define LCI_UTIL_RANDOM_HPP

namespace lci
{
extern __thread unsigned int random_seed;
static inline int rand_mt()
{
  if (LCT_unlikely(random_seed == 0)) {
    random_seed = time(NULL) + LCT_get_thread_id() + rand();
  }
  return rand_r(&random_seed);
}
}  // namespace lci

#endif  // LCI_UTIL_RANDOM_HPP