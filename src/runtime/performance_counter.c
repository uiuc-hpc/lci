#include "runtime/lcii.h"

LCII_pcounters_per_thread_t LCII_pcounters[LCI_PCOUNTER_MAX_NTHREADS];

void LCII_pcounters_init()
{
  memset(&LCII_pcounters, 0, sizeof(LCII_pcounters));
}

LCII_pcounters_per_thread_t LCII_pcounters_accumulate()
{
  LCII_pcounters_per_thread_t ret;
  memset(&ret, 0, sizeof(ret));
  for (int i = 0; i < LCIU_nthreads; ++i) {
    ret.msgs_tx += LCII_pcounters[i].msgs_tx;
    ret.bytes_tx += LCII_pcounters[i].bytes_tx;
    ret.msgs_rx += LCII_pcounters[i].msgs_rx;
    ret.bytes_rx += LCII_pcounters[i].bytes_rx;
    ret.msgs_2sided_tx += LCII_pcounters[i].msgs_2sided_tx;
    ret.msgs_2sided_rx += LCII_pcounters[i].msgs_2sided_rx;
    ret.msgs_1sided_tx += LCII_pcounters[i].msgs_1sided_tx;
    ret.msgs_1sided_rx += LCII_pcounters[i].msgs_1sided_rx;
    ret.packet_stealing += LCII_pcounters[i].packet_stealing;
    ret.progress_call += LCII_pcounters[i].progress_call;
    ret.send_succeeded_lci += LCII_pcounters[i].send_succeeded_lci;
    ret.send_failed_lci += LCII_pcounters[i].send_failed_lci;
    ret.send_failed_backend += LCII_pcounters[i].send_failed_backend;
  }
  return ret;
}

LCII_pcounters_per_thread_t LCII_pcounters_diff(LCII_pcounters_per_thread_t c1,
                                                LCII_pcounters_per_thread_t c2)
{
  LCII_pcounters_per_thread_t ret;
  memset(&ret, 0, sizeof(ret));
  for (int i = 0; i < LCIU_nthreads; ++i) {
    ret.msgs_tx = c1.msgs_tx - c2.msgs_tx;
    ret.bytes_tx = c1.bytes_tx - c2.bytes_tx;
    ret.msgs_rx = c1.msgs_rx - c2.msgs_rx;
    ret.bytes_rx = c1.bytes_rx - c2.bytes_rx;
    ret.msgs_2sided_tx = c1.msgs_2sided_tx - c2.msgs_2sided_tx;
    ret.msgs_2sided_rx = c1.msgs_2sided_rx - c2.msgs_2sided_rx;
    ret.msgs_1sided_tx = c1.msgs_1sided_tx - c2.msgs_1sided_tx;
    ret.msgs_1sided_rx = c1.msgs_1sided_rx - c2.msgs_1sided_rx;
    ret.packet_stealing = c1.packet_stealing - c2.packet_stealing;
    ret.progress_call = c1.progress_call - c2.progress_call;
    ret.send_succeeded_lci = c1.send_succeeded_lci - c2.send_succeeded_lci;
    ret.send_failed_lci = c1.send_failed_lci - c2.send_failed_lci;
    ret.send_failed_backend = c1.send_failed_backend - c2.send_failed_backend;
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
                       "\t\tprogress called: %ld;\n",
                       pcounter.progress_call);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tLCI send attempts (succeeded/failed): %ld/%ld;\n",
                       pcounter.send_succeeded_lci, pcounter.send_failed_lci);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "\t\tbackend send attempts (succeeded/failed): %ld/%ld;\n",
                       pcounter.msgs_tx, pcounter.send_failed_backend);
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  return buf;
}
