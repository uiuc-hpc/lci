// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci.hpp"
#include "lct.h"

#include <cassert>

int main(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  LCT_init();
  LCT_pmi_initialize();
  assert(LCT_pmi_initialized());

  const int lct_rank = LCT_pmi_get_rank();
  const int lct_size = LCT_pmi_get_size();

  lci::g_runtime_init();
  assert(lci::get_rank_me() == lct_rank);
  assert(lci::get_rank_n() == lct_size);
  lci::g_runtime_fina();

  // LCI should not finalize a PMI instance that was initialized by the caller.
  assert(LCT_pmi_initialized());
  assert(LCT_pmi_get_rank() == lct_rank);
  assert(LCT_pmi_get_size() == lct_size);

  LCT_pmi_finalize();
  assert(!LCT_pmi_initialized());
  LCT_fina();
  return 0;
}
