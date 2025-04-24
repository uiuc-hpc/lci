// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

namespace util
{
#if defined(__APPLE__)
// Apple is very slot with threads oversubscription
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
    if (tmp != a) {
    }
    ASSERT_EQ(tmp, a);
  }
}

}  // namespace util