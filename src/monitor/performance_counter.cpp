#include "lci_internal.hpp"

namespace lci
{
LCT_pcounter_ctx_t pcounter_ctx;

#define LCI_PCOUNTER_HANDLE_DEF(name) \
  LCT_pcounter_handle_t pcounter_handle_##name;

LCI_PCOUNTER_NONE_FOR_EACH(LCI_PCOUNTER_HANDLE_DEF)
LCI_PCOUNTER_TREND_FOR_EACH(LCI_PCOUNTER_HANDLE_DEF)
LCI_PCOUNTER_TIMER_FOR_EACH(LCI_PCOUNTER_HANDLE_DEF)

void pcounter_init()
{
#ifdef LCI_USE_PERFORMANCE_COUNTER
  pcounter_ctx = LCT_pcounter_ctx_alloc("lci");

#define LCI_PCOUNTER_NONE_REGISTER(name) \
  pcounter_handle_##name =               \
      LCT_pcounter_register(pcounter_ctx, #name, LCT_PCOUNTER_NONE);
  LCI_PCOUNTER_NONE_FOR_EACH(LCI_PCOUNTER_NONE_REGISTER)

#define LCI_PCOUNTER_TREND_REGISTER(name) \
  pcounter_handle_##name =                \
      LCT_pcounter_register(pcounter_ctx, #name, LCT_PCOUNTER_TREND);
  LCI_PCOUNTER_TREND_FOR_EACH(LCI_PCOUNTER_TREND_REGISTER)

#define LCI_PCOUNTER_TIMER_REGISTER(name) \
  pcounter_handle_##name =                \
      LCT_pcounter_register(pcounter_ctx, #name, LCT_PCOUNTER_TIMER);
  LCI_PCOUNTER_TIMER_FOR_EACH(LCI_PCOUNTER_TIMER_REGISTER)
#endif  // LCI_USE_PERFORMANCE_COUNTER
}

void pcounter_fina()
{
#ifdef LCI_USE_PERFORMANCE_COUNTER
  LCT_pcounter_ctx_free(&pcounter_ctx);
#endif
}

}  // namespace lci