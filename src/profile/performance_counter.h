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
  int64_t send_lci_failed_packet;
  int64_t send_lci_failed_bq;
  int64_t send_lci_failed_backend;
  int64_t send_backend_failed_lock;
  int64_t send_backend_failed_nomem;
  int64_t lci_cq_pop_succeeded;
  // 8x8 bytes
  int64_t lci_cq_pop_failed_empty;
  int64_t lci_cq_pop_failed_contention;
  int64_t lci_cq_pop_len_accumulated;
  int64_t progress_call;
  int64_t progress_useful_call;
  int64_t progress_useful_call_consecutive_max;
  int64_t progress_useful_call_consecutive_sum;
  int64_t recv_backend_no_packet;
  // 8x8 bytes
  int64_t backlog_queue_total_count;
  int64_t backlog_queue_send_attempts;
  int64_t backlog_queue_max_len;
  int64_t send_eager_latency_nsec_ave;      // post send -> send comp
  int64_t send_eager_latency_nsec_count;    // post send -> send comp
  int64_t send_iovec_handshake_nsec_ave;    // send rts -> recv rtr
  int64_t send_iovec_handshake_nsec_count;  // send rts -> recv rtr
  int64_t send_iovec_latency_nsec_ave;      // send rts -> send fin
  // 8x8 bytes
  int64_t send_iovec_latency_nsec_count;     // send rts -> send fin
  int64_t recv_iovec_handle_rts_nsec_ave;    // recv rts -> send rtr
  int64_t recv_iovec_handle_rts_nsec_count;  // recv rts -> send rtr
  int64_t recv_iovec_latency_nsec_ave;       // recv rts -> recv fin
  int64_t recv_iovec_latency_nsec_count;     // recv rts -> recv fin
  char padding[LCI_CACHE_LINE - (8 * 37) % LCI_CACHE_LINE];
} LCII_pcounters_per_thread_t;

#define LCI_PCOUNTER_MAX_NTHREADS 256
extern LCII_pcounters_per_thread_t LCII_pcounters[LCI_PCOUNTER_MAX_NTHREADS];

void LCII_pcounters_init();
LCII_pcounters_per_thread_t LCII_pcounters_accumulate();
LCII_pcounters_per_thread_t LCII_pcounters_diff(LCII_pcounters_per_thread_t c1,
                                                LCII_pcounters_per_thread_t c2);
char* LCII_pcounters_to_string(LCII_pcounters_per_thread_t pcounter);

#endif  // LCI_PERFORMANCE_COUNTER_H
