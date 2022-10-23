#include "lcii.h"

LCII_pcounters_per_thread_t LCII_pcounters[LCI_PCOUNTER_MAX_NTHREADS];

void LCII_pcounters_init() {
  for (int i = 0; i < LCI_PCOUNTER_MAX_NTHREADS; ++i) {
    LCII_pcounters[i].msgs_send = 0;
    LCII_pcounters[i].bytes_send = 0;
    LCII_pcounters[i].msgs_recv = 0;
    LCII_pcounters[i].bytes_recv = 0;
  }
}

LCII_pcounters_per_thread_t LCII_pcounters_accumulate() {
  LCII_pcounters_per_thread_t ret;
  memset(&ret, 0, sizeof(ret));
  for (int i = 0; i < LCIU_nthreads; ++i) {
    ret.msgs_send += LCII_pcounters[i].msgs_send;
    ret.bytes_send += LCII_pcounters[i].bytes_send;
    ret.msgs_recv += LCII_pcounters[i].msgs_recv;
    ret.bytes_recv += LCII_pcounters[i].bytes_recv;
  }
  return ret;
}