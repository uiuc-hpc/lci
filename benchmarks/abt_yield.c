/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "abt.h"

#define DEFAULT_NUM_XSTREAMS    4
#define DEFAULT_NUM_THREADS     4



inline unsigned long long cycle_time() {
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    unsigned long long cycles = ((unsigned long long)lo)|(((unsigned long long)hi)<<32);
    return cycles;
}

long *times;

ABT_mutex* mutex;
ABT_cond* cond;

volatile int total = 0;

void thread_func(void *arg)
{
    size_t myid = (size_t) arg;
    times [myid] -= cycle_time();
    ABT_thread_yield();
    times [myid] += cycle_time();
}

int compare (const void * a, const void * b) { return (*(long*) a - *(long*) b); }

int main(int argc, char *argv[])
{
    ABT_init(argc, argv);

    int i, j;
    int ret;
    int num_xstreams = DEFAULT_NUM_XSTREAMS;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1) num_xstreams = atoi(argv[1]);
    if (argc > 2) num_threads = atoi(argv[2]);

    int total_threads = num_threads * num_xstreams;
    times = (long*) malloc(sizeof(long) * total_threads);
    memset(times, 0, sizeof(long) * total_threads);

    ABT_xstream *xstreams;
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * (num_xstreams + 1));

    ABT_pool *pools;
    pools = (ABT_pool *)malloc(sizeof(ABT_pool) * (num_xstreams + 1));

    /* Initialize */

    /* Create Execution Streams */
    // ret = ABT_xstream_self(&xstreams[0]);
    for (i = 1; i < num_xstreams + 1; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
    }

    /* Get the pools attached to an execution stream */
    for (i = 1; i < num_xstreams + 1; i++) {
        ret = ABT_xstream_get_main_pools(xstreams[i], 1, pools+i);
    }

    ABT_thread thread[num_threads * num_xstreams];

    mutex = (ABT_mutex*) malloc(num_threads * num_xstreams * sizeof(ABT_mutex));
    cond = (ABT_cond*) malloc(num_threads * num_xstreams * sizeof(ABT_cond));

    for (i = 0; i < num_threads * num_xstreams; i++) {
        /* Create a mutex */
        ret = ABT_mutex_create(&mutex[i]);

        /* Create condition variables */
        ret = ABT_cond_create(&cond[i]);
    }

    int totaltest = 1000;
    long t = cycle_time();

    int time;
    for (time = 0; time < totaltest; time ++) {
        total = 0;
        /* Create threads */
        // for (i = 0; i < num_xstreams; i++) {
        for (i = 0; i < num_threads * num_xstreams; i++) {
            // size_t tid = i * num_threads + j + 1;
            ret = ABT_thread_create(pools[i % num_xstreams + 1],
                    thread_func, (void*) (size_t) i, ABT_THREAD_ATTR_NULL,
                    &thread[i]);
        }

        /* Switch to other user level threads */
        // ABT_thread_yield();

        /* Join Execution Streams */
        for (i = 0; i < num_threads * num_xstreams; i++) {
            ABT_thread_join(thread[i]);
        }
    }

    t = cycle_time() - t;
    double mean = 0;
    for (i = 0; i < total_threads; i++) {
        mean += ((double) times[i] / totaltest);
    }
    mean /= total_threads;

    double std = 0;
    for (i = 0; i < total_threads; i++) {
        std += ((mean - (double) times[i] / totaltest) *
        (mean - (double) times[i] / totaltest));
    }
    std /= (total_threads - 1);
    std = 1.96 * sqrt(std) / sqrt(total_threads);
    // qsort(times, total_threads, sizeof(long), compare);

    printf("%f\t%f\t%f\n", mean + std, mean - std, mean);
    // printf("%f\t%f\t%f\n", (double) times[total_threads-1]/totaltest,
        // (double) times[0]/totaltest, (double) times[total_threads/2]/totaltest);

    /* Free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_free(&xstreams[i]);
    }

    ABT_finalize();
    /* Finalize */
    free(pools);
    free(xstreams);

    return ret;
}
