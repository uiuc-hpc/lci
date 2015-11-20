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
long fibo[20];

worker* w;

std::atomic<int> slot;

void ffibo(intptr_t i) {
    // if (fibo[i] != 0) return;
    if (i <= 1) {
        fibo[i] = i;
    } else {
        int s1 = w[0].spawn(ffibo, i - 1);
        int s2 = w[0].spawn(ffibo, i - 2);
        w[0].join(s1);
        w[0].join(s2);
        fibo[i] = fibo[i - 1] + fibo[i - 2];
    }
}

int compare (const void * a, const void * b) { return (*(long*) a - *(long*) b); }

int main(int argc, char** args) {
    if (argc < 2) {
        printf("Usage: %s <number>\n", args[0]);
        return 1;
    }
    int nworker = 1;
    int number = atoi(args[1]);
    w = new worker[nworker];

    for (int i = 0; i < nworker; i++) {
        w[i].start();
    }
    long x = cycle_time();
    for (int tt = 0; tt < TOTAL; tt++) {
        memset(fibo, 0, sizeof(long) * number);
        int id = w[0].spawn(ffibo, number);
        w[0].join(id);
    }
    printf("RESULT: %lu %f\n", fibo[number], (double)(cycle_time() - x) / TOTAL);
    fflush(stdout);
    long y = cycle_time() - x;

    for (int i = 0; i < nworker; i++) {
        w[i].stop();
    }
    return 0;
}
