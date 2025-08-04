// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <iostream>
#include <unistd.h>
#include "lci.hpp"

// This example shows the LCI runtime lifecycle and the query of rank.

int main(int argc, char** args)
{
  char hostname[64];
  gethostname(hostname, 64);
  // Initialize the global default runtime.
  lci::g_runtime_init();
  // After at least one runtime is active, we can query the rank and nranks.
  // rank is the id of the current process
  // nranks is the total number of the processes in the current job.
  std::cout << "Hello world from rank " << lci::get_rank_me() << " of "
            << lci::get_rank_n() << " on " << hostname << std::endl;
  // Finalize the global default runtime
  lci::g_runtime_fina();
  return 0;
}
