#define BENCHMARK "OSU MPIV Multi-threaded Latency Test"
/*
 * Copyright (C) 2002-2014 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University. 
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include "mpiv.h"
#include <atomic>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define MESSAGE_ALIGNMENT 64
#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1 << 22)
#define MYBUFSIZE (MAX_MSG_SIZE + MESSAGE_ALIGNMENT)
#define SKIP_LARGE  100
#define LOOP_LARGE  1000
#define LARGE_MESSAGE_SIZE  8192

char*        s_buf1;
char*        r_buf1;
int         skip = 1000;
int         loop = 10000;


pthread_mutex_t finished_size_mutex;
pthread_cond_t  finished_size_cond;

typedef struct thread_tag {
        int id;
} thread_tag_t;

void send_thread(intptr_t arg);
void recv_thread(intptr_t arg);

#ifdef PACKAGE_VERSION
#   define HEADER "# " BENCHMARK " v" PACKAGE_VERSION "\n"
#else
#   define HEADER "# " BENCHMARK "\n"
#endif

#ifndef FIELD_WIDTH
#   define FIELD_WIDTH 20
#endif

#ifndef FLOAT_PRECISION
#   define FLOAT_PRECISION 2
#endif

int numprocs, provided, myid, err;
static int THREADS = 1;
static int WORKERS = 1;
 
int main(int argc, char *argv[])
{
    MPIV_Init(argc, argv);
    if (argc > 2) {
        THREADS = atoi(argv[1]);
        WORKERS = atoi(argv[1]);
    }

    pthread_mutex_init(&finished_size_mutex, NULL);
    pthread_cond_init(&finished_size_cond, NULL);

    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    if(numprocs != 2) {
        if(myid == 0) {
            fprintf(stderr, "This test requires exactly two processes\n");
        }

        MPIV_Finalize();
        return EXIT_FAILURE;
    }

    MPIV_Init_worker(WORKERS);

    mpiv_free(r_buf1);
    mpiv_free(s_buf1);
    MPIV_Finalize();
}

void main_task(intptr_t) {
    int i = 0;
    r_buf1 = (char*) mpiv_malloc(MYBUFSIZE);
    s_buf1 = (char*) mpiv_malloc(MYBUFSIZE);
    fult_t sr_threads[THREADS];
    thread_tag_t tags[THREADS];

    if(myid == 0) {
        fprintf(stdout, HEADER);
        fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
        fflush(stdout);

        tags[i].id = i;
        sr_threads[i] = MPIV_spawn(0, send_thread, (intptr_t) &tags[i]);
        MPIV_join(0, sr_threads[i]);
    } else {
        for(i = 0; i < THREADS; i++) {
            tags[i].id = i;
            sr_threads[i] = MPIV_spawn(i % WORKERS, recv_thread, (intptr_t) &tags[i]);
        }

        for(i = 0; i < THREADS; i++) {
            MPIV_join(i % WORKERS, sr_threads[i]);
        }
    }
}

void recv_thread(intptr_t arg) {
    int size, i, val, align_size;
    int iter;
    char *s_buf, *r_buf;
    thread_tag_t *thread_id;

    thread_id = (thread_tag_t *)arg;
    val = thread_id->id;

    align_size = MESSAGE_ALIGNMENT;

    s_buf =
        (char *) (((unsigned long) s_buf1 + (align_size - 1)) /
                  align_size * align_size);
    r_buf =
        (char *) (((unsigned long) r_buf1 + (align_size - 1)) /
                  align_size * align_size);

    for(size = MIN_MSG_SIZE, iter = 0; size <= MAX_MSG_SIZE; size = (size ? size * 2 : 1)) {
        /*finished_size ++; 
        if (finished_size == THREADS + 1) {
            done = true;
        }
        while (!done) fult_yield();
        finished_size --;
        if (finished_size == 1) {
            MPI_Barrier(MPI_COMM_WORLD);
            done = false;
        }
        while (done) fult_yield();*/
        // MPI_Barrier(MPI_COMM_WORLD);

        if(size > LARGE_MESSAGE_SIZE) {
            loop = LOOP_LARGE;
            skip = SKIP_LARGE;
        }  

#if 1
        /* touch the data */
        for(i = 0; i < size; i++) {
            s_buf[i] = 'a';
            r_buf[i] = 'b';
        }

        for(i = val; i < (loop + skip); i += THREADS) {
            MPIV_Recv (r_buf, size, MPI_CHAR, 0, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPIV_Send (s_buf, size, MPI_CHAR, 0, i, MPI_COMM_WORLD);
        }
        iter++;
#endif
    }
    // sleep(1);
}


void send_thread(intptr_t arg) {
    int size, i, val, align_size, iter;
    char *s_buf, *r_buf;
    double t_start = 0, t_end = 0, t = 0, latency;
    thread_tag_t *thread_id = (thread_tag_t *)arg;

    val = thread_id->id;
    align_size = MESSAGE_ALIGNMENT;

    s_buf =
        (char *) (((unsigned long) s_buf1 + (align_size - 1)) /
                  align_size * align_size);
    r_buf =
        (char *) (((unsigned long) r_buf1 + (align_size - 1)) /
                  align_size * align_size);

    for(size = MIN_MSG_SIZE, iter = 0; size <= MAX_MSG_SIZE; size = (size ? size * 2 : 1)) {
        // MPI_Barrier(MPI_COMM_WORLD);

        if(size > LARGE_MESSAGE_SIZE) {
            loop = LOOP_LARGE;
            skip = SKIP_LARGE;
        }  

        /* touch the data */
        for(i = 0; i < size; i++) {
            s_buf[i] = 'a';
            r_buf[i] = 'b';
        }

#if 1
        for(i = 0; i < loop + skip; i++) {
            if(i == skip) {
                t_start = MPIV_Wtime();
            }

            MPIV_Send(s_buf, size, MPI_CHAR, 1, i, MPI_COMM_WORLD);
            MPIV_Recv(r_buf, size, MPI_CHAR, 1, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
#endif

        t_end = MPIV_Wtime ();
        t = t_end - t_start;

        latency = (t) * 1.0e6 / (2.0 * loop);
        fprintf(stdout, "%-*d%*.*f\n", 10, size, FIELD_WIDTH, FLOAT_PRECISION,
                latency);
        fflush(stdout);
        iter++;
    }
}

/* vi: set sw=4 sts=4 tw=80: */
