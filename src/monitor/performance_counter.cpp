#include "lcixx_internal.hpp"

namespace lcixx
{
LCT_pcounter_ctx_t pcounter_ctx;

#define LCIXX_PCOUNTER_HANDLE_DEF(name) \
  LCT_pcounter_handle_t pcounter_handle_##name;

LCIXX_PCOUNTER_NONE_FOR_EACH(LCIXX_PCOUNTER_HANDLE_DEF)
LCIXX_PCOUNTER_TREND_FOR_EACH(LCIXX_PCOUNTER_HANDLE_DEF)
LCIXX_PCOUNTER_TIMER_FOR_EACH(LCIXX_PCOUNTER_HANDLE_DEF)

void pcounter_init()
{
#ifdef LCIXX_USE_PERFORMANCE_COUNTER
  pcounter_ctx = LCT_pcounter_ctx_alloc("lci");

#define LCIXX_PCOUNTER_NONE_REGISTER(name) \
  pcounter_handle_##name =                 \
      LCT_pcounter_register(pcounter_ctx, #name, LCT_PCOUNTER_NONE);
  LCIXX_PCOUNTER_NONE_FOR_EACH(LCIXX_PCOUNTER_NONE_REGISTER)

#define LCIXX_PCOUNTER_TREND_REGISTER(name) \
  pcounter_handle_##name =                  \
      LCT_pcounter_register(pcounter_ctx, #name, LCT_PCOUNTER_TREND);
  LCIXX_PCOUNTER_TREND_FOR_EACH(LCIXX_PCOUNTER_TREND_REGISTER)

#define LCIXX_PCOUNTER_TIMER_REGISTER(name) \
  pcounter_handle_##name =                  \
      LCT_pcounter_register(pcounter_ctx, #name, LCT_PCOUNTER_TIMER);
  LCIXX_PCOUNTER_TIMER_FOR_EACH(LCIXX_PCOUNTER_TIMER_REGISTER)
#endif  // LCIXX_USE_PERFORMANCE_COUNTER
}

void pcounter_fina()
{
#ifdef LCIXX_USE_PERFORMANCE_COUNTER
  LCT_pcounter_ctx_free(&pcounter_ctx);
#endif
}

}  // namespace lcixx