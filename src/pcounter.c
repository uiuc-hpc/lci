#include "lcii.h"

#define LCI_PCOUNTER_MAX_NTHREADS 256

LCII_pcounters_per_thread_t pcounters[LCI_PCOUNTER_MAX_NTHREADS];
__thread int thread_id;
int nthreads_total;
void LCII_pcounter_init() {

}

void LCII_pcounter_fina() {

}