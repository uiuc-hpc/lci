#include <mpi.h>
#include <omp.h>
#include <assert.h>
#include <pthread.h>
#include <limits.h>

#define MPIU_Malloc malloc
#define MPIU_Free free

// #define USE_UFCOND

#ifndef USE_UFCOND

#define MPIU_Thread_mutex_t pthread_mutex_t
#define MPIU_Thread_mutex_lock pthread_mutex_lock
#define MPIU_Thread_mutex_unlock pthread_mutex_unlock
#define MPIU_Thread_mutex_create pthread_mutex_init

#else

#define MPIU_Thread_mutex_t pthread_spinlock_t
#define MPIU_Thread_mutex_lock pthread_spin_lock
#define MPIU_Thread_mutex_unlock pthread_spin_unlock
#define MPIU_Thread_mutex_create pthread_spin_init

#endif


#define SKIP 100
#define TOTAL 500
#define NSAMPLES 10000


int NTHREADS = 2;
int SIGER = 0;

int comp (const void * elem1, const void * elem2)
{
    double f = *((double*)elem1);
    double s = *((double*)elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}

typedef struct {
    // flag for condition variable.
#ifdef USE_UFCOND
    volatile char cond_flag;
#else
    pthread_cond_t cond;
#endif
} signal_t; //__attribute__((aligned(64)));

// #define MUTEX signal[j].mutex
#define MUTEX mutex

void fcond_signal(signal_t* s) {
#ifdef USE_UFCOND
    s->cond_flag = 0;
#else
    pthread_cond_signal(&s->cond);
#endif
}

void fcond_wait(signal_t* s, MPIU_Thread_mutex_t* mutex) {
    int error;
#ifdef USE_UFCOND
    s->cond_flag = 1;
    MPIU_Thread_mutex_unlock(MUTEX); 
    while (s->cond_flag) {}

    MPIU_Thread_mutex_lock(MUTEX);
#else
    pthread_cond_wait(&s->cond, MUTEX);
#endif
}

static int flag __attribute__((aligned(64)));

int main(int argc, char** args) {
    // MPI_Init(&argc, &args);
    #pragma omp parallel
    #pragma omp master
    NTHREADS = omp_get_num_threads();

    // two threads, one wait, one signal.
    // omp_set_num_threads(NTHREADS);

    int i, error;

    MPIU_Thread_mutex_t mutex;
    MPIU_Thread_mutex_create(&MUTEX, 0);

    signal_t signal[2];
    for (i = 0; i < 2; i++) {
#ifndef USE_UFCOND
        pthread_cond_init(&(signal[i].cond), NULL);
#endif
    }
    flag = 0;

    int FIRST = 0;
    int SECOND = 0;
    double tt[NSAMPLES];
    volatile int finished = 0;

    for (SECOND = 1; SECOND < NTHREADS + 1; SECOND++)
    #pragma omp parallel
    {
        if (SECOND == NTHREADS) SECOND = 1;
        int i = 0;
        int tid = omp_get_thread_num();
        int j = (tid == FIRST)?0:1;
        int error;
        int t;

        if (tid != FIRST && tid != SECOND) {
        } else
        for (t = 0; t < NSAMPLES; t++) {
            if (tid == FIRST) {
                for (i = 0; i < SKIP + TOTAL; i++) {
                    if (i == SKIP) tt[t] = omp_get_wtime();

                    while (flag == 0) {
                        fcond_signal(&signal[1-j]);
                    }
                    MPIU_Thread_mutex_lock(&MUTEX);
                    fcond_wait(&signal[j], &MUTEX);
                    flag = 0;
                    MPIU_Thread_mutex_unlock(&MUTEX);
                }
                tt[t] = (omp_get_wtime() - tt[t]) * 1e6 / TOTAL / 2;
            } else if (tid == SECOND) {
                for (i = 0; i < SKIP + TOTAL; i++) {
                    MPIU_Thread_mutex_lock(&MUTEX);
                    fcond_wait(&signal[j], &MUTEX);
                    flag = 1;
                    MPIU_Thread_mutex_unlock(&MUTEX);
                    while (flag == 1) {
                        fcond_signal(&signal[1-j]);
                    }
                }
            }
        }

        #pragma omp barrier
        #pragma omp single
        {
            qsort(tt, NSAMPLES, sizeof(double), comp);
            double mean = 0, std = 0;
            for (t = 0; t < NSAMPLES; t++) mean += tt[t];
            mean /= NSAMPLES;
            for (t = 0; t < NSAMPLES; t++) std += (tt[t] - mean) * (tt[t] - mean);
            std /= (NSAMPLES - 1);
            std = sqrt(std);

            printf("%d %.5f %.5f\n", SECOND, mean, std);
        }
    }
    /*for (i = 0; i < NTHREADS; i++)
        if (i != SIGER)
            printf("%d-wait: %.5f (usec)\n", i,
                    (1e6 * tt[i]) /WORKS/TOTAL);*/

    return 0;
}
