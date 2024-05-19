#ifndef LC_CQ_H
#define LC_CQ_H

#ifdef LCI_USE_INLINE_CQ
typedef LCM_aqueue_t LCII_cq_t;
#endif

static inline void LCII_queue_push(LCI_comp_t cq, LCII_context_t* ctx)
{
  LCII_PCOUNTER_START(cq_push_timer);
#ifdef LCI_USE_INLINE_CQ
  LCM_aqueue_push(cq, ctx);
#else
#ifdef LCI_USE_PERFORMANCE_COUNTER
  ctx->time = LCT_now();
#endif
  LCT_queue_push(cq, ctx);
#endif
  LCII_PCOUNTER_END(cq_push_timer);
  LCII_PCOUNTER_ADD(comp_produce, 1);
}

#endif  // LC_CQ_H
