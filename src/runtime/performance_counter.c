#include "runtime/lcii.h"

LCII_pcounters_per_thread_t LCII_pcounters[LCI_PCOUNTER_MAX_NTHREADS];

void LCII_pcounters_init()
{
  memset(&LCII_pcounters, 0, sizeof(LCII_pcounters));
}

#define LCII_PCOUNTERS_FIELD_ADD(field) ret.field += LCII_pcounters[i].field
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
    LCII_PCOUNTERS_FIELD_ADD(send_lci_failed);
    LCII_PCOUNTERS_FIELD_ADD(send_backend_failed_lock);
    LCII_PCOUNTERS_FIELD_ADD(send_backend_failed_nomem);
    LCII_PCOUNTERS_FIELD_ADD(lci_cq_pop_succeeded);
    LCII_PCOUNTERS_FIELD_ADD(lci_cq_pop_failed_empty);
    LCII_PCOUNTERS_FIELD_ADD(lci_cq_pop_failed_contention);
    LCII_PCOUNTERS_FIELD_ADD(lci_cq_pop_len_accumulated);
    LCII_PCOUNTERS_FIELD_ADD(progress_call);
    LCII_PCOUNTERS_FIELD_ADD(progress_useful_call);
    LCII_PCOUNTERS_FIELD_ADD(progress_useful_call_consecutive_max);
    LCII_PCOUNTERS_FIELD_ADD(progress_useful_call_consecutive_sum);
  }
  return ret;
}

#define LCII_PCOUNTERS_FIELD_DIFF(field) ret.field = c1.field - c2.field
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
    LCII_PCOUNTERS_FIELD_DIFF(send_lci_failed);
    LCII_PCOUNTERS_FIELD_DIFF(send_backend_failed_lock);
    LCII_PCOUNTERS_FIELD_DIFF(send_backend_failed_nomem);
    LCII_PCOUNTERS_FIELD_DIFF(lci_cq_pop_succeeded);
    LCII_PCOUNTERS_FIELD_DIFF(lci_cq_pop_failed_empty);
    LCII_PCOUNTERS_FIELD_DIFF(lci_cq_pop_failed_contention);
    LCII_PCOUNTERS_FIELD_DIFF(lci_cq_pop_len_accumulated);
    LCII_PCOUNTERS_FIELD_DIFF(progress_call);
    LCII_PCOUNTERS_FIELD_DIFF(progress_useful_call);
    LCII_PCOUNTERS_FIELD_DIFF(progress_useful_call_consecutive_max);
    LCII_PCOUNTERS_FIELD_DIFF(progress_useful_call_consecutive_sum);
  }
  return ret;
}

char* LCII_pcounters_to_string(LCII_pcounters_per_thread_t pcounter) {
  static char buf[1024];
  size_t consumed = 0;
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tmessage sent (total/2sided/1sided): %ld/%ld/%ld;\n",
                       pcounter.msgs_tx, pcounter.msgs_2sided_tx,
                       pcounter.msgs_1sided_tx);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tbytes sent: %ld;\n",
                       pcounter.bytes_tx);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tmessage recv (total/2sided/1sided): %ld/%ld/%ld;\n",
                       pcounter.msgs_rx, pcounter.msgs_2sided_rx,
                       pcounter.msgs_1sided_rx);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tbytes recv: %ld;\n",
                       pcounter.bytes_rx);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tpacket stealing attempts: %ld;\n",
                       pcounter.packet_stealing);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tLCI send attempts (succeeded/failed): %ld/%ld;\n",
                       pcounter.send_lci_succeeded, pcounter.send_lci_failed);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tbackend send attempts:\n"
                       "\t\t\tSucceeded: %ld\n"
                       "\t\t\tFailed due to lock: %ld\n"
                       "\t\t\tFailed due to no memory: %ld\n",
                       pcounter.msgs_tx,
                       pcounter.send_backend_failed_lock,
                       pcounter.send_backend_failed_nomem);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tLCI cq pop:\n"
                       "\t\t\tSucceeded: %ld\n"
                       "\t\t\tFailed due to empty queue: %ld\n"
                       "\t\t\tFailed due to thread contention: %ld\n"
                       "\t\t\tPending counts accumulated: %ld\n",
                       pcounter.lci_cq_pop_succeeded,
                       pcounter.lci_cq_pop_failed_empty,
                       pcounter.lci_cq_pop_failed_contention,
                       pcounter.lci_cq_pop_len_accumulated);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tLCI progress function:\n"
                       "\t\t\tCalled: %ld\n"
                       "\t\t\tUseful calls: %ld\n"
                       "\t\t\tConsecutive useful calls (max/sum): %ld/%ld\n",
                       pcounter.progress_call,
                       pcounter.progress_useful_call,
                       pcounter.progress_useful_call_consecutive_max,
                       pcounter.progress_useful_call_consecutive_sum);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  return buf;
}
