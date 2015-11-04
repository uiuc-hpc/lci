#include <stdio.h>
#include <thread>
#include <string.h>
#include <assert.h>
#include <atomic>
#include <sys/time.h>
#include <unistd.h>
#include <mpi.h>

#include "fult.h"
#include "comm_queue.h"

#include "comm_exp.h"

inline unsigned long long cycle_time() {
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    unsigned long long cycles = ((unsigned long long)lo)|(((unsigned long long)hi)<<32);
    return cycles;
}

double* start, *end;

worker* w;

std::atomic<bool> data;

mpsc_queue_t<void*> q;

static int SIZE = 1;

thread_local void* buffer = NULL;

void wait_comm(intptr_t i) {
    if (buffer == NULL) {
        buffer = malloc(SIZE);
    }
    start[i] = MPI_Wtime();
    fult_sync s(buffer, SIZE, 1, i);
    q.enqueue((void*) &s);
    s.wait();
    end[i] = MPI_Wtime();
}

int main(int argc, char** args) {
    int provide;
    MPI_Init_thread(&argc, &args, MPI_THREAD_MULTIPLE, &provide);
    if (argc < 3) {
        printf("%s <nworker> <nthreads>", args[0]);
        return -1;
    }

    int nworker = atoi(args[1]);
    int nthreads = atoi(args[2]);
    SIZE = atoi(args[3]);
    // q.init();

    int total_threads = nworker * nthreads;

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    volatile bool stop_comm = false;
    start = (double *) std::malloc(total_threads * sizeof(double));
    end = (double *) std::malloc(total_threads * sizeof(double));

    if (rank == 0) {
        // start the comm threads
        std::thread comm_thread([&stop_comm] {
            while (!stop_comm) {
                void* elm = NULL;
                while (q.dequeue(elm)) {
                    fult_sync* sync = (fult_sync*) elm;
                    int flag = 0;
                    MPI_Iprobe(1, sync->tag, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
                    if (flag) {
                        MPI_Recv((void*) sync->buffer, SIZE, MPI_BYTE, 1, sync->tag,
                            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        sync->signal();
                    } else {
                        q.enqueue(sync);
                    }
                }
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
        }

        for (int i = 0; i < nworker; i++) {
            w[i].stop();
        }

        stop_comm = true;
        comm_thread.join();
        printf("%f\n", times * 1e6 / TOTAL / total_threads);
    } else {
        double t1 = 0;
        void* buf = std::malloc(SIZE);
        for (int t = 0; t < TOTAL + SKIP; t++) {
            if (t == SKIP) t1 = MPI_Wtime();
            MPI_Barrier(MPI_COMM_WORLD);
            for (int i = 0; i < total_threads; i++) {
                MPI_Send(buf, SIZE, MPI_BYTE, 0, i, MPI_COMM_WORLD);
            }
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
