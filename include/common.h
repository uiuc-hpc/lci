#ifndef COMMON_H_
#define COMMON_H_

#include "config.h"
#include <stdexcept>
#define ALIGNED64(x) (((x) + 63) / 64 * 64)

#ifdef USE_TIMING
/** Setup timing */
static double tbl_timing;
static double signal_timing;
static double memcpy_timing;
static double wake_timing;
static double post_timing;
static double misc_timing;
static double poll_timing;
static double rdma_timing;

static int eventSetP;
static long long t_valueP[3], t0_valueP[3], t1_valueP[3];

#define initt(x) double x = 0;
#define startt(x) \
  { x -= MPIV_Wtime(); }
#define stopt(x) \
  { x += MPIV_Wtime(); }
#define resett(x) \
  { x = 0; }
#else
#define initt(x) \
  {}
#define startt(x) \
  {}
#define stopt(x) \
  {}
#define resett(x) \
  {}
#endif

#endif
