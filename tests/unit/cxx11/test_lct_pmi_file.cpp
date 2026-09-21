// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lct.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
  assert(argc == 2);
  const int expected_size = std::atoi(argv[1]);

  LCT_init();
  LCT_pmi_initialize();
  assert(LCT_pmi_initialized());

  const int rank = LCT_pmi_get_rank();
  assert(rank >= 0 && rank < expected_size);
  assert(LCT_pmi_get_size() == expected_size);

  char key[LCT_PMI_STRING_LIMIT];
  char value[LCT_PMI_STRING_LIMIT];
  std::snprintf(key, sizeof(key), "file-test-rank-%d", rank);
  std::snprintf(value, sizeof(value), "value-from-%d", rank);
  LCT_pmi_publish(key, value);
  LCT_pmi_barrier();

  for (int i = 0; i < expected_size; ++i) {
    char query_key[LCT_PMI_STRING_LIMIT];
    char query_value[LCT_PMI_STRING_LIMIT] = {};
    char expected[LCT_PMI_STRING_LIMIT];
    std::snprintf(query_key, sizeof(query_key), "file-test-rank-%d", i);
    std::snprintf(expected, sizeof(expected), "value-from-%d", i);
    LCT_pmi_getname(i, query_key, query_value);
    assert(std::strcmp(query_value, expected) == 0);
  }

  LCT_pmi_barrier();
  LCT_pmi_finalize();
  assert(!LCT_pmi_initialized());
  LCT_fina();
  return 0;
}
