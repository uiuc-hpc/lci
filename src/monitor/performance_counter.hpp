#ifndef LCI_PERFORMANCE_COUNTER_HPP
#define LCI_PERFORMANCE_COUNTER_HPP

namespace lci
{
extern LCT_pcounter_ctx_t pcounter_ctx;

// clang-format off
#define LCI_PCOUNTER_NONE_FOR_EACH(_macro) 

#define LCI_PCOUNTER_TREND_FOR_EACH(_macro) \
    _macro(communicate_ok)                    \
    _macro(communicate_posted)                \
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
    _macro(backlog_queue_push)           \
    _macro(backlog_queue_pop)           \
    _macro(retry_due_to_backlog_queue)           \
    _macro(progress)

#define LCI_PCOUNTER_TIMER_FOR_EACH(_macro)
// clang-format on

#define LCI_PCOUNTER_HANDLE_DECL(name) \
  extern LCT_pcounter_handle_t pcounter_handle_##name;

LCI_PCOUNTER_NONE_FOR_EACH(LCI_PCOUNTER_HANDLE_DECL)
LCI_PCOUNTER_TREND_FOR_EACH(LCI_PCOUNTER_HANDLE_DECL)
LCI_PCOUNTER_TIMER_FOR_EACH(LCI_PCOUNTER_HANDLE_DECL)

#ifdef LCI_USE_PERFORMANCE_COUNTER
#define LCI_PCOUNTER_ADD(name, val) \
  LCT_pcounter_add(pcounter_ctx, pcounter_handle_##name, val);
#define LCI_PCOUNTER_START(name) \
  LCT_pcounter_start(pcounter_ctx, pcounter_handle_##name);
#define LCI_PCOUNTER_END(name) \
  LCT_pcounter_end(pcounter_ctx, pcounter_handle_##name);
#define LCI_PCOUNTER_NOW(time) LCT_time_t time = LCT_now();
#define LCI_PCOUNTER_STARTT(name, time) \
  LCT_pcounter_startt(pcounter_ctx, pcounter_handle_##name, time);
#define LCI_PCOUNTER_ENDT(name, time) \
  LCT_pcounter_endt(pcounter_ctx, pcounter_handle_##name, time);
#define LCI_PCOUNTER_SINCE(name, time) \
  LCT_pcounter_add(pcounter_ctx, pcounter_handle_##name, LCT_now() - time);
#else
#define LCI_PCOUNTER_ADD(name, val)
#define LCI_PCOUNTER_START(name)
#define LCI_PCOUNTER_END(name)
#define LCI_PCOUNTER_NOW(time)
#define LCI_PCOUNTER_STARTT(name, time)
#define LCI_PCOUNTER_ENDT(name, time)
#define LCI_PCOUNTER_SINCE(name, time)
#endif

void pcounter_init();
void pcounter_fina();

}  // namespace lci

#endif  // LCI_PERFORMANCE_COUNTER_HPP
