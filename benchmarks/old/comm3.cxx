#include <assert.h>
#include <atomic>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>

#include "fult.h"

typedef fult_sync MPIV_Request;

#include "comm_exp.h"
#include "comm_queue.h"
#include "mpiv.h"

#if 0
#undef TOTAL
#define TOTAL 20
#undef SKIP
#define SKIP 0
#endif

double *start, *end;
worker* w;

static int SIZE = 1;

void wait_comm(intptr_t i)
{
  void* buffer = mpiv_malloc(SIZE);
  start[i] = MPI_Wtime();
  MPIV_Recv2(buffer, SIZE, 1, i);
  end[i] = MPI_Wtime();
  mpiv_free(buffer);
}

int main(int argc, char** args)
{
  MPIV_Init(argc, args);

  if (argc < 3) {
    printf("%s <nworker> <nthreads>", args[0]);
    return -1;
  }

  int nworker = atoi(args[1]);
  int nthreads = atoi(args[2]);
  SIZE = atoi(args[3]);

  int total_threads = nworker * nthreads;
  start = (double*)malloc(total_threads * sizeof(double));
  end = (double*)malloc(total_threads * sizeof(double));

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  volatile bool stop_comm = false;
  // start the comm threads
  std::thread comm_thread([&stop_comm] {
    while (!stop_comm) {
      MPIV_Progress();
    }
  });

  if (rank == 0) {
    w = new worker[nworker];
    for (int i = 0; i < nworker; i++) {
      w[i].start();
    }

    double times = 0;
    for (int t = 0; t < TOTAL + SKIP; t++) {
      MPIV_Flush();
      MPI_Send(0, 0, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
      for (int i = 0; i < total_threads; i++) {
        w[i % nworker].spawn_to(i / nworker, wait_comm, i);
      }

      double min = 2e9;
      double max = 0;
      for (int i = 0; i < total_threads; i++) {
        w[i % nworker].join(i / nworker);
        min = std::min(start[i], min);
        max = std::max(end[i], max);
      }
      if (t >= SKIP) times += (max - min);
    }

    for (int i = 0; i < nworker; i++) {
      w[i].stop();
    }

    printf("%f\n", times * 1e6 / TOTAL / total_threads);
  } else {
    double t1 = 0;
    void* buf = mpiv_malloc(SIZE);
    for (int t = 0; t < TOTAL + SKIP; t++) {
      if (t == SKIP) t1 = MPI_Wtime();
      MPIV_Flush();
      MPI_Recv(0, 0, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      for (int i = 0; i < total_threads; i++) {
        MPIV_Send(buf, SIZE, 0, i);
      }
    }
    t1 = MPI_Wtime() - t1;
    // printf("Send time: %f\n", t1 * 1e6 / TOTAL / total_threads);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  stop_comm = true;
  comm_thread.join();

#if 0
    if (rank == 0) {
        double mean = 0;
        for (int i = 0; i < total_threads; i++) {
            mean += ((double) times[i] * 1e6 / TOTAL);
        }
        mean /= total_threads;

        double std = 0;
        for (int i = 0; i < total_threads; i++) {
            std += (mean - (double) times[i] / TOTAL) * (mean - (double) times[i]/ TOTAL);
        }
        std /= (total_threads - 1);
        std = 1.96 * sqrt(std) / sqrt(total_threads);

        printf("%f\t%f\t%f\n", mean + std, mean - std, mean);
    }
#endif
  MPIV_Finalize();

  return 0;
}
