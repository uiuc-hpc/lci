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
    if (myid == 0) {
        while (!total) {
            ABT_thread_yield();
        }
    } else {
        total = 1;
    }
}

int compare (const void * a, const void * b) { return (*(long*) a - *(long*) b); }

int main(int argc, char *argv[])
{
    ABT_init(argc, argv);

    int i, j;
    int ret;
    int num_xstreams = 1;
    int num_threads = 2;

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

    int totaltest = 1000;
    long t = cycle_time();

    int time;
    for (time = 0; time < totaltest; time ++) {
        total = 0;
        /* Create threads */
        // for (i = 0; i < num_xstreams; i++) {
        for (i = 0; i < 2; i++) {
            // size_t tid = i * num_threads + j + 1;
            ret = ABT_thread_create(pools[1],
                    thread_func, (void*) (size_t) i, ABT_THREAD_ATTR_NULL,
                    &thread[i]);
        }

        /* Switch to other user level threads */
        // ABT_thread_yield();

        /* Join Execution Streams */
        for (i = 0; i < 2; i++) {
            ABT_thread_join(thread[i]);
        }
    }

    t = cycle_time() - t;

    printf("%f\n", (double) t / totaltest);

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
