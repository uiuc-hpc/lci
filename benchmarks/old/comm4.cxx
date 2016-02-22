#include <stdio.h>
#include <thread>
#include <string.h>
#include <assert.h>
#include <atomic>
#include <sys/time.h>
#include <unistd.h>
#include <mpi.h>

// #define CHECK_RESULT
#include <papi.h>

#include "fult.h"

#include "mpiv.h"
#include "comm_queue.h"
#include "comm_exp.h"

#if 0
#undef TOTAL
#define TOTAL 20
#undef SKIP
#define SKIP 0
#endif

#define MAX_MSG_SIZE (256*1024)

double* start, *end;
worker* w;

static int SIZE = 1;
// thread_local void* buffer = NULL;

void* alldata;
int total_threads;
int eventSet;
long long t_value[3], t0_value[3], t1_value[3];

void wait_comm(intptr_t i) {
    char* buffer = (char*) ((uintptr_t) alldata + SIZE * i);

#ifdef CHECK_RESULT
    memset(buffer, 'A', SIZE);
#endif

#if 0
    PAPI_read(eventSet, t0_value);
#endif

    // start[i] = MPIV_Wtime();
    MPIV_Recv(buffer, SIZE, 1, i);
    MPIV_Send(buffer, SIZE, 1, i);
    // end[i] = MPIV_Wtime();

#if 0
    PAPI_read(eventSet, t1_value);
    for (int j=0; j<3; j++)
        t_value[j] += (t1_value[j] - t0_value[j]);
#endif 

#ifdef CHECK_RESULT
    for (int j = 0 ; j < SIZE; j++) {
        assert(buffer[j] == 'B');
    }
#endif
}

void send_comm(intptr_t) {
#if 0
    PAPI_read(eventSet, t0_value);
#endif

    for (int i = 0; i < total_threads; i++) {
        char* buf = (char*) ((uintptr_t) alldata + SIZE * i);
#ifdef CHECK_RESULT
        memset(buf, 'B', SIZE);
#endif
        MPIV_Send(buf, SIZE, 0, i);
        MPIV_Recv(buf, SIZE, 0, i);
    }

#if 0
    PAPI_read(eventSet, t1_value);
    for (int j=0; j<3; j++)
        t_value[j] += (t1_value[j] - t0_value[j]);
#endif 

}

int main(int argc, char** args) {

#if 0
    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_thread_init(pthread_self);
#endif 
 
    MPIV_Init(argc, args);

    if (argc < 3) {
        printf("%s <nworker> <nthreads>", args[0]);
        return -1;
    }

    int nworker = atoi(args[1]);
    int nthreads = atoi(args[2]);

    total_threads = nworker * nthreads;
    start = (double *) std::malloc(total_threads * sizeof(double));
    end = (double *) std::malloc(total_threads * sizeof(double));

    int rank = MPIV.me;

    alldata = (void*) mpiv_malloc((size_t) MAX_MSG_SIZE*total_threads);

#if 1
    int* threads = (int*) malloc(sizeof(int) * total_threads);
    if (rank == 0) {
        w = new worker[nworker];
        for (int i = 0; i < nworker; i++) {
            w[i].start();
        }
    } else {
        w = new worker[1];
        w[0].start();
    }
#endif
    double times = 0;

#if 0
    eventSet = PAPI_NULL;
   
    PAPI_create_eventset(&eventSet);
    PAPI_add_event(eventSet, PAPI_L1_DCM);
    PAPI_add_event(eventSet, PAPI_L2_DCM);
    PAPI_add_event(eventSet, PAPI_L3_TCM);
#endif
    long long papi_vals[3];

    for (SIZE=1; SIZE<=MAX_MSG_SIZE; SIZE<<=1) {
        if (rank == 0) {
            times = 0;
            for (int t = 0; t < TOTAL + SKIP; t++) {
                // MPI_Barrier(MPI_COMM_WORLD);
                MPIV_Send(0, 0, 1, total_threads + 1); 

                if (t == SKIP) {
                    resett(tbl_timing);
                    resett(memcpy_timing);
                    resett(misc_timing);
                    resett(wake_timing);
                    resett(signal_timing);
                    resett(poll_timing);
                    resett(post_timing);
                    //memset(t_value, 0, 3 * sizeof(long long));
                    //PAPI_start(eventSet);
                    //PAPI_read(eventSetP, t0_value);
                    //memcpy(t0_value, t_valueP, 3 * sizeof(long long));
                }
                for (int i = 0; i < total_threads; i++) {
                    threads[i] = w[i % nworker].spawn(wait_comm, i);
                }
                double min = 2e9;
                double max = 0;
                for (int i = 0; i < total_threads; i++) {
                    w[i % nworker].join(threads[i]);
                    #if 0
                    min = std::min(start[i], min);
                    max = std::max(end[i], max);
                    #endif
                }
                #if 0
                if (t >= SKIP)
                    times += (max - min);
                #endif
            }
            #if 0
            PAPI_stop(eventSet, papi_vals);
            PAPI_read(eventSetP, t1_value);
            memcpy(t1_value, t_valueP, 3 * sizeof(long long));

            for (int j = 0; j < 3; j++) 
                t_value[j] = (t1_value[j] - t0_value[j]);
            #endif

#if 0
            // times = MPIV_Wtime() - times;
            std::cout << "[" <<
                SIZE << "] " <<
                times * 1e6 / TOTAL / total_threads / 2 << " " <<
                #ifdef USE_TIMING
                tbl_timing * 1e6 / TOTAL / total_threads << " " <<
                signal_timing * 1e6 / TOTAL / total_threads << " " <<
                wake_timing * 1e6 / TOTAL / total_threads <<  " " <<
                memcpy_timing * 1e6 / TOTAL / total_threads <<  " " <<
                post_timing * 1e6 / TOTAL / total_threads << " " <<
                misc_timing * 1e6 / TOTAL / total_threads << " " <<
                #ifdef USE_PAPI
                1.0 * t_value[0] / TOTAL / total_threads << " " <<
                1.0 * t_value[1] / TOTAL / total_threads << " " <<
                1.0 * t_value[2] / TOTAL / total_threads <<
                #endif
                #endif
                std::endl;
#endif
        } else {
            for (int t = 0; t < TOTAL + SKIP; t++) {
                if (t == SKIP) {
                    times = MPIV_Wtime();
                    #if 0
                    resett(rdma_timing);
                    PAPI_start(eventSet);
                    // memcpy(t0_value, t_valueP, 3 * sizeof(long long));
                    memset(t_value, 0, 3* sizeof(long long));
                    #endif
                }

                MPIV_Recv(0, 0, 0, total_threads + 1);

                int tid = w[0].spawn(send_comm, 0);
                w[0].join(tid);
            }
            times = MPIV_Wtime() - times;
            // memcpy(t1_value, t_valueP, 3 * sizeof(long long));

            //for (int j = 0; j < 3; j++) 
            //    t_value[j] = (t1_value[j] - t0_value[j]);

            // PAPI_stop(eventSet, papi_vals);

            printf("[%d] %f\n", SIZE, times * 1e6 / TOTAL / total_threads / 2);
        }
    }
#if 1
    if ( rank == 0 ) {
        for (int i = 0; i < nworker; i++) {
            w[i].stop();
        }
    } else {
        w[0].stop();
    }
#endif
    mpiv_free(alldata);

    MPIV_Finalize();

    return 0;
}
