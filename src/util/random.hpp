#ifndef LCIXX_UTIL_RANDOM_HPP
#define LCIXX_UTIL_RANDOM_HPP

namespace lcixx
{
extern __thread unsigned int random_seed;
static inline int rand_mt()
{
  if (LCT_unlikely(random_seed == 0)) {
    random_seed = time(NULL) + LCT_get_thread_id() + rand();
  }
  return rand_r(&random_seed);
}
}  // namespace lcixx

#endif  // LCIXX_UTIL_RANDOM_HPP