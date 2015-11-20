#include <stdio.h>
#include <thread>
#include <string.h>
#include <assert.h>
#include <atomic>

#include "fult.h"

#define TOTAL 1000

inline unsigned long long cycle_time() {
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    unsigned long long cycles = ((unsigned long long)lo)|(((unsigned long long)hi)<<32);
    return cycles;
}

long* times;

void f1(intptr_t i) {
    times[i] -= cycle_time();
    fult_yield();
    times[i] += cycle_time();
}

int compare (const void * a, const void * b) { return (*(long*) a - *(long*) b); }

int main(int argc, char** args) {
    if (argc < 2) {
        printf("Usage %s <num_workers> <num_threads>\n", args[0]);
    }
    int nworker = atoi(args[1]);
    int num_threads = atoi(args[2]);
    assert(num_threads <= 512 && "Unsupported # threads");

    int total_threads = num_threads * nworker;
    times = (long *) std::malloc(total_threads * sizeof(long));
    memset(times, 0, sizeof(long) * total_threads);

    worker w[nworker];

    for (int i = 0; i < nworker; i++) {
        w[i].start();
    }

    long x = cycle_time();
    for (int tt = 0; tt < TOTAL; tt++) {
        for (int i = 0; i < num_threads * nworker; i++) {
            w[i % nworker].spawn(f1, i);
        }
        for (int i = 0; i < num_threads * nworker; i++) {
            w[i % nworker].join(i / nworker);
        }
    }
    long y = cycle_time() - x;

    // std::qsort(times, num_threads * nworker, sizeof(long), compare);
    double mean = 0;
    for (int i = 0; i < total_threads; i++) {
        mean += ((double) times[i] / TOTAL);
    }
    mean /= total_threads;

    double std = 0;
    for (int i = 0; i < total_threads; i++) {
        std += (mean - (double) times[i] / TOTAL) * (mean - (double) times[i]/ TOTAL);
    }
    std /= (total_threads - 1);
    std = 1.96 * sqrt(std) / sqrt(total_threads);

    // qsort(times, total_threads, sizeof(long), compare);

    printf("%f\t%f\t%f\n", mean + std, mean - std, mean);
    // printf("%f\t%f\t%f\n", (double) times[total_threads-1]/TOTAL,
    //    (double) times[0]/TOTAL, (double) times[total_threads/2]/TOTAL);

    for (int i = 0; i < nworker; i++) {
        w[i].stop();
    }
    return 0;
}
