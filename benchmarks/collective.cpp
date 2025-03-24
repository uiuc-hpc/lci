#include <iostream>
#include <thread>
#include <cassert>
#include <chrono>
#include <atomic>
#include "lct.h"
#include "lci.hpp"

bool is_blocking = false;
const int nsteps = 10000;

int main(int argc, char** args)
{
  if (argc > 1) {
    is_blocking = atoi(args[1]);
  }

  lci::g_runtime_init();

  lci::barrier();
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < nsteps; ++i) {
    if (is_blocking) {
      lci::barrier();
    } else {
      lci::comp_t comp = lci::alloc_sync();
      lci::barrier_x().comp(comp)();
      while (!lci::sync_test(comp, nullptr)) {
        lci::progress();
      }
      lci::free_comp(&comp);
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  if (lci::get_rank() == 0) {
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Blocking: " << is_blocking << std::endl;
    std::cout << "Number of steps: " << nsteps << std::endl;
    std::cout << "Elapsed time: " << elapsed.count() << " s\n";
    std::cout << "Time per barrier: " << elapsed.count() * 1e6 / nsteps << " us\n";
  }

  lci::g_runtime_fina();
  return 0;
}
