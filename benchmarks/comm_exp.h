#ifndef COMM_EXP_H_
#define COMM_EXP_H_

#include <sys/time.h>

#define LARGE 8192

#define NEXP 10

#define TOTAL 10000
#define SKIP 1000

#define TOTAL_LARGE 1000
#define SKIP_LARGE 100

#define DEFAULT_NUM_WORKER 4
#define DEFAULT_NUM_THREAD 4

inline double wtime()
{
  struct timeval t1;
  gettimeofday(&t1, 0);
  return t1.tv_sec + t1.tv_usec / 1e6;
}

inline double wutime()
{
  struct timeval t1;
  gettimeofday(&t1, 0);
  return t1.tv_sec * 1e6 + t1.tv_usec;
}

#define max(a, b) ((a > b) ? (a) : (b))

#endif
