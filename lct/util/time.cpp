#include <unistd.h>
#include <cstdio>
#include "lcti.hpp"

LCT_time_t LCT_now()
{
  struct timespec t1;
  int ret = clock_gettime(CLOCK_MONOTONIC, &t1);
  if (ret != 0) {
    fprintf(stderr, "Cannot get time!\n");
    abort();
  }
  return (LCT_time_t)t1.tv_sec * (LCT_time_t)1e9 + (LCT_time_t)t1.tv_nsec;
}

double LCT_time_to_ns(LCT_time_t time) { return (double)time; }

double LCT_time_to_us(LCT_time_t time) { return (double)time / 1e3; }

double LCT_time_to_ms(LCT_time_t time) { return (double)time / 1e6; }

double LCT_time_to_s(LCT_time_t time) { return (double)time / 1e9; }

// LCT_time_t LCT_now() { return LCII_ucs_get_time(); }
//
// double LCT_time_to_ns(LCT_time_t time) { return LCII_ucs_time_to_nsec(time);
// }
//
// double LCT_time_to_us(LCT_time_t time) { return LCII_ucs_time_to_usec(time);
// }
//
// double LCT_time_to_ms(LCT_time_t time) { return LCII_ucs_time_to_msec(time);
// }
//
// double LCT_time_to_s(LCT_time_t time) { return LCII_ucs_time_to_sec(time); }