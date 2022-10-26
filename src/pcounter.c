#include "lcii.h"

LCII_pcounters_per_thread_t LCII_pcounters[LCI_PCOUNTER_MAX_NTHREADS];

void LCII_pcounters_init() {
  for (int i = 0; i < LCI_PCOUNTER_MAX_NTHREADS; ++i) {
    LCII_pcounters[i].msgs_tx = 0;
    LCII_pcounters[i].bytes_tx = 0;
    LCII_pcounters[i].msgs_rx = 0;
    LCII_pcounters[i].bytes_rx = 0;
    LCII_pcounters[i].msgs_2sided_tx = 0;
    LCII_pcounters[i].msgs_2sided_rx = 0;
    LCII_pcounters[i].msgs_1sided_tx = 0;
    LCII_pcounters[i].msgs_1sided_rx = 0;
    LCII_pcounters[i].packet_stealing = 0;
    LCII_pcounters[i].progress_call = 0;
  }
}

LCII_pcounters_per_thread_t LCII_pcounters_accumulate() {
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
  }
  return ret;
}