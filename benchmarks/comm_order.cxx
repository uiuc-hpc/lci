#include <stdio.h>
#include <thread>
#include <string.h>
#include <assert.h>
#include <atomic>
#include <sys/time.h>
#include <unistd.h>
#include <mpi.h>

#include "fult.h"

typedef fult_sync MPIV_Request;

#include "mpiv.h"
#include "comm_queue.h"
#include "comm_exp.h"

double* start, *end;
worker* w;

static int SIZE = 1;
thread_local void* buffer = NULL;

void wait_comm(intptr_t i) {
    if (buffer == NULL) {
        buffer = malloc(SIZE);
    }
    start[i] = MPI_Wtime();
    MPIV_Recv(buffer, SIZE, 1, i);
    end[i] = MPI_Wtime();
}

int main(int argc, char** args) {
    int provide;
    MPI_Init_thread(&argc, &args, MPI_THREAD_MULTIPLE, &provide);
    if (argc < 3) {
        printf("%s <nworker> <nthreads> <SIZE>\n", args[0]);
        return -1;
    }

    int nworker = atoi(args[1]);
    int nthreads = atoi(args[2]);
    SIZE = atoi(args[3]);

    int total_threads = nworker * nthreads;
    start = (double *) std::malloc(total_threads * sizeof(double));
    end = (double *) std::malloc(total_threads * sizeof(double));

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    volatile bool stop_comm = false;

    if (rank == 0) {
        // start the comm threads
        std::thread comm_thread([&stop_comm] {
            while (!stop_comm) {
                MPI_Progress();
            }
        });
        w = new worker[nworker];
        for (int i = 0; i < nworker; i++) {
            w[i].start();
        }

        double times = 0;
        for (int t = 0; t < TOTAL + SKIP; t++) {
            MPI_Barrier(MPI_COMM_WORLD);
            for (int i = 0; i < total_threads; i++) {
                w[i % nworker].fult_new(i / nworker, wait_comm, i);
            }

            double min = 2e9;
            double max = 0;
            for (int i = 0; i < total_threads; i++) {
                w[i % nworker].fult_join(i / nworker);
                min = std::min(start[i], min);
                max = std::max(end[i], max);
            }
            if (t >= SKIP)
                times += (max - min);
            MPI_Barrier(MPI_COMM_WORLD);
        }

        for (int i = 0; i < nworker; i++) {
            w[i].stop();
        }

        stop_comm = true;
        comm_thread.join();
        printf("%f\n", times * 1e6 / TOTAL / total_threads);
    } else {
        double t1 = 0;
        void* buf = malloc(SIZE);
        for (int t = 0; t < TOTAL + SKIP; t++) {
            if (t == SKIP)
                t1 = MPI_Wtime();
            for (int i = total_threads - 1; i >= 0; i--) {
                MPI_Send(buf, SIZE, MPI_BYTE, 0, i, MPI_COMM_WORLD);
            }
            MPI_Barrier(MPI_COMM_WORLD);
            MPI_Barrier(MPI_COMM_WORLD);
        }
        t1 = MPI_Wtime() - t1;
        // printf("Send time: %f\n", t1 * 1e6 / TOTAL / total_threads);
    }

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
    MPI_Finalize();

    return 0;
}
