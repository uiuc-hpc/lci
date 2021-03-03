#ifndef COMM_EXP_H_
#define COMM_EXP_H_

#include <assert.h>

#define MIN_MSG (1)
#define MAX_MSG (1*1024*1024)
#define NUM_THREADS 3
#define LARGE 8192
#define TOTAL 4000
#define TOTAL_LARGE 1000

// Always enable assert
#ifdef NDEBUG
#undef NDEBUG
#endif

void write_buffer(char* buffer, int len, char input) {
  for (int i = 0; i < len; ++i) {
    buffer[i] = input;
  }
}

void check_buffer(const char* buffer, int len, char expect) {
  for (int i = 0; i < len; ++i) {
    if (buffer[i] != expect) {
      printf("check_buffer failed!\n");
      abort();
    }
  }
}

#ifdef __cplusplus
#include <omp.h>

namespace omp {

    typedef void* (*func_t)(void*);

    static inline void thread_run(func_t f, int n)
    {
#pragma omp parallel num_threads(n)
      {
        f((void*)0);
      }
    }

    static inline int thread_id()
    {
      return omp_get_thread_num();
    }

    static inline int thread_count()
    {
      return omp_get_num_threads();
    }

    static inline void thread_barrier()
    {
#pragma omp barrier
    }

}
#endif
#endif
