#include "runtime/lcii.h"

LCII_pcounters_per_thread_t LCII_pcounters[LCI_PCOUNTER_MAX_NTHREADS]
    __attribute__((aligned(LCI_CACHE_LINE)));

void LCII_pcounters_init()
{
  memset(&LCII_pcounters, 0, sizeof(LCII_pcounters));
}

#define LCII_PCOUNTERS_FIELD_ADD(field) ret.field += LCII_pcounters[i].field
#define LCII_PCOUNTERS_FIELD_MAX(field) \
  LCIU_MAX_ASSIGN(ret.field, LCII_pcounters[i].field)
#define LCII_PCOUNTERS_FIELD_AVE(ave, count)                       \
  LCIU_update_average(&ret.ave, &ret.count, LCII_pcounters[i].ave, \
                      LCII_pcounters[i].count)
LCII_pcounters_per_thread_t LCII_pcounters_accumulate()
{
  LCII_pcounters_per_thread_t ret;
  memset(&ret, 0, sizeof(ret));
  for (int i = 0; i < LCIU_nthreads; ++i) {
    LCII_PCOUNTERS_FIELD_ADD(msgs_tx);
    LCII_PCOUNTERS_FIELD_ADD(bytes_tx);
    LCII_PCOUNTERS_FIELD_ADD(msgs_rx);
    LCII_PCOUNTERS_FIELD_ADD(bytes_rx);
    LCII_PCOUNTERS_FIELD_ADD(msgs_2sided_tx);
    LCII_PCOUNTERS_FIELD_ADD(msgs_2sided_rx);
    LCII_PCOUNTERS_FIELD_ADD(msgs_1sided_tx);
    LCII_PCOUNTERS_FIELD_ADD(msgs_1sided_rx);
    LCII_PCOUNTERS_FIELD_ADD(packet_stealing);
    LCII_PCOUNTERS_FIELD_ADD(send_lci_succeeded);
    LCII_PCOUNTERS_FIELD_ADD(send_lci_failed_packet);
    LCII_PCOUNTERS_FIELD_ADD(send_lci_failed_bq);
    LCII_PCOUNTERS_FIELD_ADD(send_lci_failed_backend);
    LCII_PCOUNTERS_FIELD_ADD(send_backend_failed_lock);
    LCII_PCOUNTERS_FIELD_ADD(send_backend_failed_nomem);
    LCII_PCOUNTERS_FIELD_ADD(lci_cq_pop_succeeded);
    LCII_PCOUNTERS_FIELD_ADD(lci_cq_pop_failed_empty);
    LCII_PCOUNTERS_FIELD_ADD(lci_cq_pop_failed_contention);
    LCII_PCOUNTERS_FIELD_ADD(lci_cq_pop_len_accumulated);
    LCII_PCOUNTERS_FIELD_ADD(progress_call);
    LCII_PCOUNTERS_FIELD_ADD(progress_useful_call);
    LCII_PCOUNTERS_FIELD_MAX(progress_useful_call_consecutive_max);
    LCII_PCOUNTERS_FIELD_ADD(progress_useful_call_consecutive_sum);
    LCII_PCOUNTERS_FIELD_ADD(recv_backend_no_packet);
    LCII_PCOUNTERS_FIELD_ADD(backlog_queue_total_count);
    LCII_PCOUNTERS_FIELD_ADD(backlog_queue_send_attempts);
    LCII_PCOUNTERS_FIELD_MAX(backlog_queue_max_len);
    LCII_PCOUNTERS_FIELD_AVE(send_eager_latency_nsec_ave,
                             send_eager_latency_nsec_count);
    LCII_PCOUNTERS_FIELD_AVE(send_iovec_handshake_nsec_ave,
                             send_iovec_handshake_nsec_count);
    LCII_PCOUNTERS_FIELD_AVE(send_iovec_latency_nsec_ave,
                             send_iovec_latency_nsec_count);
    LCII_PCOUNTERS_FIELD_AVE(recv_iovec_handle_rts_nsec_ave,
                             recv_iovec_handle_rts_nsec_count);
    LCII_PCOUNTERS_FIELD_AVE(recv_iovec_latency_nsec_ave,
                             recv_iovec_latency_nsec_count);
  }
  return ret;
}

#define LCII_PCOUNTERS_FIELD_KEEP(field) ret.field = c1.field
#define LCII_PCOUNTERS_FIELD_DIFF(field) ret.field = (c1.field - c2.field)
LCII_pcounters_per_thread_t LCII_pcounters_diff(LCII_pcounters_per_thread_t c1,
                                                LCII_pcounters_per_thread_t c2)
{
  LCII_pcounters_per_thread_t ret;
  memset(&ret, 0, sizeof(ret));
  for (int i = 0; i < LCIU_nthreads; ++i) {
    LCII_PCOUNTERS_FIELD_DIFF(msgs_tx);
    LCII_PCOUNTERS_FIELD_DIFF(bytes_tx);
    LCII_PCOUNTERS_FIELD_DIFF(msgs_rx);
    LCII_PCOUNTERS_FIELD_DIFF(bytes_rx);
    LCII_PCOUNTERS_FIELD_DIFF(msgs_2sided_tx);
    LCII_PCOUNTERS_FIELD_DIFF(msgs_2sided_rx);
    LCII_PCOUNTERS_FIELD_DIFF(msgs_1sided_tx);
    LCII_PCOUNTERS_FIELD_DIFF(msgs_1sided_rx);
    LCII_PCOUNTERS_FIELD_DIFF(packet_stealing);
    LCII_PCOUNTERS_FIELD_DIFF(send_lci_succeeded);
    LCII_PCOUNTERS_FIELD_DIFF(send_lci_failed_packet);
    LCII_PCOUNTERS_FIELD_DIFF(send_lci_failed_bq);
    LCII_PCOUNTERS_FIELD_DIFF(send_lci_failed_backend);
    LCII_PCOUNTERS_FIELD_DIFF(send_backend_failed_lock);
    LCII_PCOUNTERS_FIELD_DIFF(send_backend_failed_nomem);
    LCII_PCOUNTERS_FIELD_DIFF(lci_cq_pop_succeeded);
    LCII_PCOUNTERS_FIELD_DIFF(lci_cq_pop_failed_empty);
    LCII_PCOUNTERS_FIELD_DIFF(lci_cq_pop_failed_contention);
    LCII_PCOUNTERS_FIELD_DIFF(lci_cq_pop_len_accumulated);
    LCII_PCOUNTERS_FIELD_DIFF(progress_call);
    LCII_PCOUNTERS_FIELD_DIFF(progress_useful_call);
    LCII_PCOUNTERS_FIELD_KEEP(progress_useful_call_consecutive_max);
    LCII_PCOUNTERS_FIELD_DIFF(progress_useful_call_consecutive_sum);
    LCII_PCOUNTERS_FIELD_DIFF(recv_backend_no_packet);
    LCII_PCOUNTERS_FIELD_DIFF(backlog_queue_total_count);
    LCII_PCOUNTERS_FIELD_DIFF(backlog_queue_send_attempts);
    LCII_PCOUNTERS_FIELD_KEEP(backlog_queue_max_len);
    LCII_PCOUNTERS_FIELD_KEEP(send_eager_latency_nsec_ave);
    LCII_PCOUNTERS_FIELD_DIFF(send_eager_latency_nsec_count);
    LCII_PCOUNTERS_FIELD_KEEP(send_iovec_handshake_nsec_ave);
    LCII_PCOUNTERS_FIELD_DIFF(send_iovec_handshake_nsec_count);
    LCII_PCOUNTERS_FIELD_KEEP(send_iovec_latency_nsec_ave);
    LCII_PCOUNTERS_FIELD_DIFF(send_iovec_latency_nsec_count);
    LCII_PCOUNTERS_FIELD_KEEP(recv_iovec_handle_rts_nsec_ave);
    LCII_PCOUNTERS_FIELD_DIFF(recv_iovec_handle_rts_nsec_count);
    LCII_PCOUNTERS_FIELD_KEEP(recv_iovec_latency_nsec_ave);
    LCII_PCOUNTERS_FIELD_DIFF(recv_iovec_latency_nsec_count);
  }
  return ret;
}

#define LCII_PCOUNTERS_FIELD_TO_STRING(field, annotation)                     \
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed, "%d,%s,%ld\n", \
                       LCI_RANK, annotation, pcounter.field);                 \
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
char* LCII_pcounters_to_string(LCII_pcounters_per_thread_t pcounter)
{
  static char buf[2048];
  size_t consumed = 0;
  LCII_PCOUNTERS_FIELD_TO_STRING(msgs_tx, "Total message sent");
  LCII_PCOUNTERS_FIELD_TO_STRING(msgs_2sided_tx, "2sided message sent");
  LCII_PCOUNTERS_FIELD_TO_STRING(msgs_1sided_tx, "1sided message sent");
  LCII_PCOUNTERS_FIELD_TO_STRING(bytes_tx, "Bytes sent");
  LCII_PCOUNTERS_FIELD_TO_STRING(msgs_rx, "Total message recved");
  LCII_PCOUNTERS_FIELD_TO_STRING(msgs_2sided_rx, "2sided message recved");
  LCII_PCOUNTERS_FIELD_TO_STRING(msgs_1sided_rx, "1sided message recved");
  LCII_PCOUNTERS_FIELD_TO_STRING(bytes_rx, "Bytes recved");
  LCII_PCOUNTERS_FIELD_TO_STRING(packet_stealing, "Packet stealing attempts");
  LCII_PCOUNTERS_FIELD_TO_STRING(send_lci_succeeded,
                                 "LCI send attempts succeeded");
  LCII_PCOUNTERS_FIELD_TO_STRING(send_lci_failed_packet,
                                 "LCI send attempts failed due to no packet");
  LCII_PCOUNTERS_FIELD_TO_STRING(
      send_lci_failed_bq,
      "LCI send attempts failed due to non-empty backlog queue");
  LCII_PCOUNTERS_FIELD_TO_STRING(
      send_lci_failed_backend,
      "LCI send attempts failed due to failed backend send");
  LCII_PCOUNTERS_FIELD_TO_STRING(msgs_tx, "Backend send attempts succeeded");
  LCII_PCOUNTERS_FIELD_TO_STRING(send_backend_failed_lock,
                                 "Backend send attempts failed due to lock");
  LCII_PCOUNTERS_FIELD_TO_STRING(
      send_backend_failed_nomem,
      "Backend send attempts failed due to no memory");
  LCII_PCOUNTERS_FIELD_TO_STRING(lci_cq_pop_succeeded, "LCI cq pop succeeded");
  LCII_PCOUNTERS_FIELD_TO_STRING(lci_cq_pop_failed_empty,
                                 "LCI cq pop failed due to empty queue");
  LCII_PCOUNTERS_FIELD_TO_STRING(lci_cq_pop_failed_contention,
                                 "LCI cq pop failed due to thread contention");
  LCII_PCOUNTERS_FIELD_TO_STRING(lci_cq_pop_len_accumulated,
                                 "LCI cq pop pending counts accumulated");
  LCII_PCOUNTERS_FIELD_TO_STRING(progress_call, "LCI progress function called");
  LCII_PCOUNTERS_FIELD_TO_STRING(progress_useful_call,
                                 "LCI progress function useful calls");
  LCII_PCOUNTERS_FIELD_TO_STRING(
      progress_useful_call_consecutive_max,
      "LCI progress function Consecutive useful calls max");
  LCII_PCOUNTERS_FIELD_TO_STRING(
      progress_useful_call_consecutive_sum,
      "LCI progress function Consecutive useful calls sum");
  LCII_PCOUNTERS_FIELD_TO_STRING(
      recv_backend_no_packet,
      "Backend post recv attempts failed due to no packet");
  LCII_PCOUNTERS_FIELD_TO_STRING(backlog_queue_total_count,
                                 "Backlog queue total item count");
  LCII_PCOUNTERS_FIELD_TO_STRING(backlog_queue_max_len,
                                 "Backlog queue maximum length");
  LCII_PCOUNTERS_FIELD_TO_STRING(backlog_queue_send_attempts,
                                 "Backlog queue send attempts");
  LCII_PCOUNTERS_FIELD_TO_STRING(send_eager_latency_nsec_ave,
                                 "Send eager time average");
  LCII_PCOUNTERS_FIELD_TO_STRING(send_eager_latency_nsec_count,
                                 "Send eager time count");
  LCII_PCOUNTERS_FIELD_TO_STRING(send_iovec_handshake_nsec_ave,
                                 "Send iovec handshake time average");
  LCII_PCOUNTERS_FIELD_TO_STRING(send_iovec_handshake_nsec_count,
                                 "Send iovec handshake time count");
  LCII_PCOUNTERS_FIELD_TO_STRING(send_iovec_latency_nsec_ave,
                                 "Send iovec time average");
  LCII_PCOUNTERS_FIELD_TO_STRING(send_iovec_latency_nsec_count,
                                 "Send iovec time count");
  LCII_PCOUNTERS_FIELD_TO_STRING(recv_iovec_handle_rts_nsec_ave,
                                 "Recv iovec handle rts time average");
  LCII_PCOUNTERS_FIELD_TO_STRING(recv_iovec_handle_rts_nsec_count,
                                 "Recv iovec handle rts time count");
  LCII_PCOUNTERS_FIELD_TO_STRING(recv_iovec_latency_nsec_ave,
                                 "Recv iovec time average");
  LCII_PCOUNTERS_FIELD_TO_STRING(recv_iovec_latency_nsec_count,
                                 "Recv iovec time count");
  return buf;
}
