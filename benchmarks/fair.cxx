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

std::atomic<bool> data;
std::atomic<bool> spawned;

void fair(intptr_t i) {
    spawned = true;
    if (i == 0) {
        while (!data) {
            fult_yield();
        };
    } else {
        data = true;
    }
}

int compare (const void * a, const void * b) { return (*(long*) a - *(long*) b); }

int main(int argc, char** args) {
    int nworker = 1;
    w = new worker[nworker];

    for (int i = 0; i < nworker; i++) {
        w[i].start();
    }

    long x = cycle_time();
    for (int tt = 0; tt < TOTAL; tt++) {
        data = false;
        spawned = false;
        w[0].spawn_to(0, fair, 0);
        while (!spawned) {};
        w[0].spawn_to(63, fair, 1);
        w[0].join(0);
        w[0].join(63);
    }
    printf("RESULT: %f\n", (double)(cycle_time() - x) / TOTAL);
    fflush(stdout);

    for (int i = 0; i < nworker; i++) {
        w[i].stop();
    }
    return 0;
}
