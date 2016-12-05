#include "pool.h"
#define USE_PAPI
#include "profiler.h"
#include "assert.h"
#include "affinity.h"

#include <stdio.h>
#include <thread>

#define TIME 100000
#define NP 100

int main(int argc, char** args) {
  mv_pool* p;
  mv_pool_create(&p, 4, 1024);
  profiler_init();
  profiler f({PAPI_L1_TCM, PAPI_TOT_CYC});
  f.start();
  {
    affinity::set_me_to(0);
    auto t1 = std::thread([=] {
        affinity::set_me_to(9);
        for (int i = 0; i < TIME; i++){
          void* d[NP];
          for (int j = 0; j < NP; j++) {
            d[j] = mv_pool_get(p);
          }
          for (int j = 0; j < NP; j++) {
            mv_pool_put(p, d[j]);
          }
        }
    });
    for (int i = 0; i < TIME; i++){
      void* d[NP];
      for (int j = 0; j < NP; j++) {
        d[j] = mv_pool_get(p);
      }
      for (int j = 0; j < NP; j++) {
        mv_pool_put(p, d[j]);
      }
    }
    t1.join();
  }
  auto& x = f.stop();
  printf("%.5f %.5f\n", 1.0 * x[0]/TIME, 1.0 * x[1]/TIME);
  return 0;
}
