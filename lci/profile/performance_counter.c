#include "runtime/lcii.h"

LCT_pcounter_ctx_t LCII_pcounter_ctx;

#define LCII_PCOUNTER_HANDLE_DEF(name) \
  LCT_pcounter_handle_t LCII_pcounter_handle_##name;

LCII_PCOUNTER_NONE_FOR_EACH(LCII_PCOUNTER_HANDLE_DEF)
LCII_PCOUNTER_TREND_FOR_EACH(LCII_PCOUNTER_HANDLE_DEF)
LCII_PCOUNTER_TIMER_FOR_EACH(LCII_PCOUNTER_HANDLE_DEF)

void LCII_pcounters_init()
{
#ifdef LCI_USE_PERFORMANCE_COUNTER
  LCII_pcounter_ctx = LCT_pcounter_ctx_alloc("lci");

#define LCII_PCOUNTER_NONE_REGISTER(name) \
  LCII_pcounter_handle_##name =           \
      LCT_pcounter_register(LCII_pcounter_ctx, #name, LCT_PCOUNTER_NONE);
  LCII_PCOUNTER_NONE_FOR_EACH(LCII_PCOUNTER_NONE_REGISTER)

#define LCII_PCOUNTER_TREND_REGISTER(name) \
  LCII_pcounter_handle_##name =            \
      LCT_pcounter_register(LCII_pcounter_ctx, #name, LCT_PCOUNTER_TREND);
  LCII_PCOUNTER_TREND_FOR_EACH(LCII_PCOUNTER_TREND_REGISTER)

#define LCII_PCOUNTER_TIMER_REGISTER(name) \
  LCII_pcounter_handle_##name =            \
      LCT_pcounter_register(LCII_pcounter_ctx, #name, LCT_PCOUNTER_TIMER);
  LCII_PCOUNTER_TIMER_FOR_EACH(LCII_PCOUNTER_TIMER_REGISTER)
#endif  // LCI_USE_PERFORMANCE_COUNTER
}

void LCII_pcounters_fina()
{
#ifdef LCI_USE_PERFORMANCE_COUNTER
  LCT_pcounter_ctx_free(&LCII_pcounter_ctx);
#endif
}