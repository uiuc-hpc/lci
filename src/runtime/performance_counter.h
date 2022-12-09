#ifndef LCI_PERFORMANCE_COUNTER_H
#define LCI_PERFORMANCE_COUNTER_H

#ifdef LCI_USE_PERFORMANCE_COUNTER
#define LCII_PCOUNTERS_WRAPPER(stat) stat
#else
#define LCII_PCOUNTERS_WRAPPER(stat)
#endif

typedef struct {
  int64_t msgs_tx;
  int64_t bytes_tx;
  int64_t msgs_rx;
  int64_t bytes_rx;
  int64_t msgs_2sided_tx;
  int64_t msgs_1sided_tx;
  int64_t msgs_2sided_rx;
  int64_t msgs_1sided_rx;
  int64_t packet_stealing;
  int64_t progress_call;
  char padding[48];
} LCII_pcounters_per_thread_t __attribute__((aligned(64)));

#define LCI_PCOUNTER_MAX_NTHREADS 256
extern LCII_pcounters_per_thread_t LCII_pcounters[LCI_PCOUNTER_MAX_NTHREADS];

void LCII_pcounters_init();
LCII_pcounters_per_thread_t LCII_pcounters_accumulate();

#endif  // LCI_PERFORMANCE_COUNTER_H
