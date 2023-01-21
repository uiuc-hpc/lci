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
  // 8x8 bytes
  int64_t packet_stealing;
  int64_t send_lci_succeeded;
  int64_t send_lci_failed;
  int64_t send_backend_failed_lock;
  int64_t send_backend_failed_nomem;
  int64_t lci_cq_pop_succeeded;
  int64_t lci_cq_pop_failed_empty;
  int64_t lci_cq_pop_failed_contention;
  // 8x8 bytes
  int64_t lci_cq_pop_len_accumulated;
  int64_t progress_call;
  int64_t progress_useful_call;
  int64_t progress_useful_call_consecutive_max;
  int64_t progress_useful_call_consecutive_sum;
  char padding[LCI_CACHE_LINE - (8 * 21) % LCI_CACHE_LINE];
} LCII_pcounters_per_thread_t __attribute__((aligned(LCI_CACHE_LINE)));

#define LCI_PCOUNTER_MAX_NTHREADS 256
extern LCII_pcounters_per_thread_t LCII_pcounters[LCI_PCOUNTER_MAX_NTHREADS];

void LCII_pcounters_init();
LCII_pcounters_per_thread_t LCII_pcounters_accumulate();
LCII_pcounters_per_thread_t LCII_pcounters_diff(LCII_pcounters_per_thread_t c1,
                                                LCII_pcounters_per_thread_t c2);
char* LCII_pcounters_to_string(LCII_pcounters_per_thread_t pcounter);

#endif  // LCI_PERFORMANCE_COUNTER_H
