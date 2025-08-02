// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#define KEEP_RETRY(status_var, stmt) \
  do {                               \
    status_var = stmt;               \
    lci::progress();                 \
  } while ((status_var).is_retry())

namespace util
{
#if defined(__APPLE__)
// Apple is very slow with threads oversubscription
// potentiall due to no spinlock support.
const int NTHREADS = 4;
const int NITERS_LARGE = 1000;
const int NITERS = 400;
const int NITERS_SMALL = 40;
#else
const int NTHREADS = 10;
const int NITERS_LARGE = 100000;
const int NITERS = 10000;
const int NITERS_SMALL = 1000;
#endif

void write_buffer(void* buffer, size_t size, const char a)
{
  memset(buffer, a, size);
}

void check_buffer(void* buffer, size_t size, const char a)
{
  for (size_t i = 0; i < size; i++) {
    char tmp = ((char*)buffer)[i];
    ASSERT_EQ(tmp, a);
  }
}

template <typename Fn, typename... Args>
void spawn_threads(int nthreads, Fn&& fn, Args&&... args)
{
  if (nthreads == 1) {
    fn(0, args...);
    return;
  }

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(fn, i, args...);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
}

}  // namespace util