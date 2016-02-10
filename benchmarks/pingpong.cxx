#include <stdio.h>
#include <thread>
#include <string.h>
#include <assert.h>
#include <atomic>
#include <sys/time.h>
#include <unistd.h>
#include <mpi.h>

// #define CHECK_RESULT

#include "mpiv.h"
#include "comm_queue.h"
#include "comm_exp.h"

#include "profiler.h"

#if 0
#undef TOTAL
#define TOTAL 20
#undef SKIP
#define SKIP 0
#endif

#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE 4*1024*1024
// #(256*1024)

double* start, *end;

static int SIZE = 1;
// thread_local void* buffer = NULL;

void* alldata;
int total_threads;

void wait_comm(intptr_t i) {
  char* buffer = (char*) ((uintptr_t) alldata + SIZE * i);

#ifdef CHECK_RESULT
  memset(buffer, 'A', SIZE);
#endif

  MPIV_Recv(buffer, SIZE, 1, i);
  MPIV_Send(buffer, SIZE, 1, i);

#ifdef CHECK_RESULT
  for (int j = 0 ; j < SIZE; j++) {
    assert(buffer[j] == 'B');
  }
#endif
}

void send_comm(intptr_t) {
  for (int i = 0; i < total_threads; i++) {
    char* buf = (char*) ((uintptr_t) alldata + SIZE * i);
#ifdef CHECK_RESULT
    memset(buf, 'B', SIZE);
#endif
    MPIV_Send(buf, SIZE, 0, i);
    MPIV_Recv(buf, SIZE, 0, i);
  }
}

int nworkers;
int nthreads;
int size;

int main(int argc, char** args) {
  MPIV_Init(argc, args);

  if (argc < 3) {
    printf("%s <nworkers> <nthreads>", args[0]);
    return -1;
  }

  nworkers = atoi(args[1]);
  nthreads = atoi(args[2]);
  size = atoi(args[3]);

  if (MPIV.me == 0) {
    MPIV_Init_worker(nworkers);
  } else {
    MPIV_Init_worker(1);
  }

  MPIV_Finalize();
  return 0;
}

void main_task(intptr_t arg) {
  double times = 0;
  int rank = MPIV.me;
  total_threads = nworkers * nthreads;
  start = (double *) std::malloc(total_threads * sizeof(double));
  end = (double *) std::malloc(total_threads * sizeof(double));
  alldata = (void*) mpiv_malloc((size_t) MAX_MSG_SIZE*total_threads);
  int* threads = (int*) malloc(sizeof(int) * total_threads);

  for (SIZE=MIN_MSG_SIZE; SIZE<=MAX_MSG_SIZE; SIZE<<=1) {
    memset(alldata, 'a', SIZE * total_threads);
    if (rank == 0) {
      times = 0;
      for (int t = 0; t < TOTAL + SKIP; t++) {
        //MPI_Barrier(MPI_COMM_WORLD);
        MPIV_Send(0, 0, 1, total_threads + 1); 
        // MPIV_Recv(0, 0, 1, total_threads + 2);

        if (t == SKIP) {
          resett(tbl_timing);
          resett(memcpy_timing);
          resett(misc_timing);
          resett(wake_timing);
          resett(signal_timing);
          resett(poll_timing);
          resett(post_timing);
          times = MPIV_Wtime();
        }
        for (int i = 0; i < total_threads; i++) {
          threads[i] = MPIV_spawn(i % nworkers, wait_comm, i);
        }
        for (int i = 0; i < total_threads; i++) {
          MPIV_join(i % nworkers, threads[i]);
        }
      }

#if defined(USE_TIMING) || defined(USE_PAPI)
      times = MPIV_Wtime() - times;
      std::cout << std::fixed << "[" <<
        SIZE << "] " <<
        times * 1e6 / TOTAL / total_threads / 2 << " " <<
#ifdef USE_TIMING
        tbl_timing * 1e6 / TOTAL / total_threads << " " <<
        signal_timing * 1e6 / TOTAL / total_threads << " " <<
        wake_timing * 1e6 / TOTAL / total_threads <<  " " <<
        memcpy_timing * 1e6 / TOTAL / total_threads <<  " " <<
        post_timing * 1e6 / TOTAL / total_threads << " " <<
        misc_timing * 1e6 / TOTAL / total_threads << " " <<
#endif
        std::endl;
#endif
    } else {
      for (int t = 0; t < TOTAL + SKIP; t++) {
        if (t == SKIP) {
          times = MPIV_Wtime();
        }
        MPIV_Recv(0, 0, 0, total_threads + 1);
        // MPIV_Send(0, 0, 0, total_threads + 2);

        int tid = MPIV_spawn(0, send_comm, 0);
        MPIV_join(0, tid);
      }
      times = MPIV_Wtime() - times;

#if !defined(USE_PAPI) && !defined(USE_TIMING)
      printf("[%d] %f\n", SIZE, times * 1e6 / TOTAL / total_threads / 2);
#endif
    }
  }
  mpiv_free(alldata);
}
