#ifndef LCI_PCOUNTER_H
#define LCI_PCOUNTER_H

#ifdef LCI_USE_PERFORMANCE_COUNTER
#define LCII_PCOUNTERS_WRAPPER(stat) stat
#else
#define LCII_PCOUNTERS_WRAPPER(stat)
#endif

typedef struct {
  int64_t msgs_send;
  int64_t bytes_send;
  int64_t msgs_recv;
  int64_t bytes_recv;
  char padding[32];
} LCII_pcounters_per_thread_t __attribute__((aligned(64)));

#define LCI_PCOUNTER_MAX_NTHREADS 256
extern LCII_pcounters_per_thread_t LCII_pcounters[LCI_PCOUNTER_MAX_NTHREADS];


void LCII_pcounters_init();
LCII_pcounters_per_thread_t LCII_pcounters_accumulate();

#endif  // LCI_PCOUNTER_H
