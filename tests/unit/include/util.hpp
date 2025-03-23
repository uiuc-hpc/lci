// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

namespace util
{
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