#ifndef LCI_TRACER_H
#define LCI_TRACER_H

extern LCT_tracer_t LCII_tracer;

#ifdef LCI_USE_TRACER
#define LCII_TRACER_RECORD(op, type, rank, size) \
  LCT_tracer_##op(LCII_tracer, type, rank, size);
#else
#define LCII_TRACER_RECORD(op, type, rank, size)
#endif

void LCII_tracer_init();
void LCII_tracer_fina();

#endif  // LCI_TRACER_H
