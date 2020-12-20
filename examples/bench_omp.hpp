#include <omp.h>
#ifdef USE_PAPI
#include <papi.h>
#endif

namespace omp {

  typedef void* (*func_t)(void*);

  static inline void thread_init()
  {
    #ifdef USE_PAPI
    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_thread_init(pthread_self);
    #endif
  }

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
