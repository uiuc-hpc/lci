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

double wtime() {
  using namespace std::chrono;
  return duration_cast<duration<double> >(
             high_resolution_clock::now().time_since_epoch())
      .count();
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
        fibo[i] = 0;
        fult_t s1 = w[0].spawn(ffibo, i - 1);
        fult_t s2 = w[0].spawn(ffibo, i - 2);
        w[0].join(s1);
        w[0].join(s2);
        fibo[i] = fibo[i - 1] + fibo[i - 2];
    }
}

int compare (const void * a, const void * b) { return (*(long*) a - *(long*) b); }

int number;
int nworker;

void main_task(intptr_t args) {
  worker* w = (worker*) args;
  double t = wtime();
  for (int tt = 0; tt < TOTAL; tt++) {
    ffibo(number);
  }
  printf("RESULT: %lu %f\n", fibo[number], (double) 1e6 * (wtime() - t) / TOTAL);
  w[0].stop_main();
}

int main(int argc, char** args) {
    if (argc < 2) {
        printf("Usage: %s <number>\n", args[0]);
        return 1;
    }
    nworker = 1;
    number = atoi(args[1]);
    w = ::new worker[nworker];
    for (int i = 1; i < nworker; i++) {
        w[i].start();
    }
    w[0].start_main(main_task, (intptr_t) w);
    for (int i = 1; i < nworker; i++) {
        w[i].stop();
    }
    return 0;
}
