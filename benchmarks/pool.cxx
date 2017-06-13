#define USE_AFFI
#define USE_PAPI

#include "mv.h"
#include "pool.h"
#include "mv/profiler.h"

__thread int mv_core_id = -1;

#include <stdio.h>

#define TIME 100000
#define NP 100

mv_pool* p;

void* test_put(void* arg) {
  set_me_to_(0);
  for (int i = 0; i < TIME; i++){
    void* d[NP];
    for (int j = 0; j < NP; j++) {
      d[j] = mv_pool_get(p);
    }
    for (int j = 0; j < NP; j++) {
      mv_pool_put(p, d[j]);
    }
  }
  return 0;
}

int main(int argc, char** args) {
  mv_pool_init();
  void * data = malloc(4 * 1024);
  mv_pool_create(&p, data, 4, 1024);

  profiler_init();
  profiler f({PAPI_L2_TCM});
  f.start();
  {
    pthread_t t1;
    pthread_create(&t1, 0, test_put, 0);
    test_put(0);
    pthread_join(t1, 0);
  }
  auto& x = f.stop();
  printf("%.5f\n", 1.0 * x[0]/TIME);//, 1.0 * x[1]/TIME);
  return 0;
}
