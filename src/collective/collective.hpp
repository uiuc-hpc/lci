// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_COLLECTIVE_HPP
#define LCI_COLLECTIVE_HPP

namespace lci
{
extern std::atomic<uint64_t> g_sequence_number;

static inline uint64_t get_sequence_number()
{
  const int MAX_SEQUENCE_NUMBER = 65536;
  return g_sequence_number.fetch_add(1, std::memory_order_relaxed) %
         MAX_SEQUENCE_NUMBER;
}

}  // namespace lci

#endif  // LCI_COLLECTIVE_HPP