#ifndef LCIXX_PERFORMANCE_COUNTER_HPP
#define LCIXX_PERFORMANCE_COUNTER_HPP

namespace lcixx
{
extern LCT_pcounter_ctx_t pcounter_ctx;

// clang-format off
#define LCIXX_PCOUNTER_NONE_FOR_EACH(_macro) 

#define LCIXX_PCOUNTER_TREND_FOR_EACH(_macro) \
    _macro(communicate)                       \
    _macro(communicate_retry)                 \
    _macro(net_send_post)                     \
    _macro(net_send_post_retry)               \
    _macro(net_send_comp)                     \
    _macro(net_recv_post)                     \
    _macro(net_recv_post_retry)               \
    _macro(net_recv_comp)                     \
    _macro(packet_get)                        \
    _macro(packet_get_retry)                  \
    _macro(packet_put)                        \
    _macro(comp_produce)                      \
    _macro(comp_consume)                      \
    _macro(net_poll_cq_entry_count)           \
    _macro(progress)

#define LCIXX_PCOUNTER_TIMER_FOR_EACH(_macro)
// clang-format on

#define LCIXX_PCOUNTER_HANDLE_DECL(name) \
  extern LCT_pcounter_handle_t pcounter_handle_##name;

LCIXX_PCOUNTER_NONE_FOR_EACH(LCIXX_PCOUNTER_HANDLE_DECL)
LCIXX_PCOUNTER_TREND_FOR_EACH(LCIXX_PCOUNTER_HANDLE_DECL)
LCIXX_PCOUNTER_TIMER_FOR_EACH(LCIXX_PCOUNTER_HANDLE_DECL)

#ifdef LCIXX_USE_PERFORMANCE_COUNTER
#define LCIXX_PCOUNTER_ADD(name, val) \
  LCT_pcounter_add(pcounter_ctx, pcounter_handle_##name, val);
#define LCIXX_PCOUNTER_START(name) \
  LCT_pcounter_start(pcounter_ctx, pcounter_handle_##name);
#define LCIXX_PCOUNTER_END(name) \
  LCT_pcounter_end(pcounter_ctx, pcounter_handle_##name);
#define LCIXX_PCOUNTER_NOW(time) LCT_time_t time = LCT_now();
#define LCIXX_PCOUNTER_STARTT(name, time) \
  LCT_pcounter_startt(pcounter_ctx, pcounter_handle_##name, time);
#define LCIXX_PCOUNTER_ENDT(name, time) \
  LCT_pcounter_endt(pcounter_ctx, pcounter_handle_##name, time);
#define LCIXX_PCOUNTER_SINCE(name, time) \
  LCT_pcounter_add(pcounter_ctx, pcounter_handle_##name, LCT_now() - time);
#else
#define LCIXX_PCOUNTER_ADD(name, val)
#define LCIXX_PCOUNTER_START(name)
#define LCIXX_PCOUNTER_END(name)
#define LCIXX_PCOUNTER_NOW(time)
#define LCIXX_PCOUNTER_STARTT(name, time)
#define LCIXX_PCOUNTER_ENDT(name, time)
#define LCIXX_PCOUNTER_SINCE(name, time)
#endif

void pcounter_init();
void pcounter_fina();

}  // namespace lcixx

#endif  // LCIXX_PERFORMANCE_COUNTER_HPP
