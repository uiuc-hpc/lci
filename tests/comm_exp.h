#ifndef COMM_EXP_H_
#define COMM_EXP_H_

#include <assert.h>

#define MIN_MSG (1)
#define MAX_MSG (1*1024*1024)

#define LARGE 8192
#define TOTAL 4000
#define TOTAL_LARGE 1000

// Always enable assert
#ifdef NDEBUG
#undef NDEBUG
#endif

#endif
