#ifndef LCIX_COLL_H
#define LCIX_COLL_H

#include "lci.h"
#include "runtime/lcii.h"
#include <stdatomic.h>

/* many collective algorithms are logarithmic, this is useful for that
 * returns integer base-2 log of value: (1 << ilog2(value)) <= value
 * invalid at value == 0, log(0) doesn't make any sense anyway */
static inline int ilog2(int value)
{
  return 8 * sizeof(int) - __builtin_clz(value) - 1;
}

typedef enum {
  LCIXC_COLL_SEND = 0,
  LCIXC_COLL_RECV,
  LCIXC_COLL_SENDRECV,
  LCIXC_COLL_OP,
  LCIXC_COLL_MEM,
  LCIXC_COLL_FREE,
} LCIXC_coll_sched_type_t;

struct LCIXC_mcoll_sched_t {
  LCIXC_coll_sched_type_t type;
  int rank;
  LCI_mbuffer_t src;
  LCI_mbuffer_t dst;
};
typedef struct LCIXC_mcoll_sched_t LCIXC_mcoll_sched_t;

struct LCIXC_lcoll_sched_t {
  LCIXC_coll_sched_type_t type;
  int rank;
  LCI_lbuffer_t src;
  LCI_lbuffer_t dst;
};
typedef struct LCIXC_lcoll_sched_t LCIXC_lcoll_sched_t;

struct LCIX_collective_s {
  int cur;
  int total;
  LCI_data_type_t type;
  LCI_tag_t tag;
  LCI_endpoint_t ep;
  LCI_op_t op;
  LCI_comp_t user_comp;
  void* user_context;
  union {
    LCI_mbuffer_t mbuffer;
    LCI_lbuffer_t lbuffer;
  } data;
  union {
    LCIXC_mcoll_sched_t* msched;
    LCIXC_lcoll_sched_t* lsched;
  } next;
  LCI_comp_t send_comp;
  LCI_comp_t recv_comp;
  atomic_size_t inprogress;
};

void LCIXC_coll_handler(LCI_request_t req);

static inline LCI_comp_t LCIXC_coll_comp(LCI_comp_type_t type,
                                         LCI_device_t device)
{
  LCI_comp_t comp;
  switch (type) {
    case LCI_COMPLETION_QUEUE:
      LCI_queue_create(device, &comp);
      break;
    case LCI_COMPLETION_HANDLER:
      LCI_handler_create(device, LCIXC_coll_handler, &comp);
      break;
    case LCI_COMPLETION_SYNC:
      LCI_sync_create(device, 1, &comp);
      break;
  }
  return comp;
}

static inline void LCIXC_coll_comp_free(LCI_comp_type_t type,
                                        LCI_comp_t* completion)
{
  switch (type) {
    case LCI_COMPLETION_QUEUE:
      LCI_queue_free(completion);
      break;
    case LCI_COMPLETION_HANDLER:
      break;
    case LCI_COMPLETION_SYNC:
      LCI_sync_free(completion);
      break;
  }
}

static inline void LCIXC_mcoll_init(LCIX_collective_t coll, LCI_endpoint_t ep,
                                    LCI_tag_t tag, LCI_op_t op, LCI_comp_t comp,
                                    void* user_context, LCI_mbuffer_t data,
                                    size_t sched_ops)
{
  coll->cur = 0;
  coll->total = 0;
  coll->type = LCI_MEDIUM;
  coll->tag = tag;
  coll->ep = ep;
  coll->op = op;
  coll->user_comp = comp;
  coll->user_context = user_context;
  coll->data.mbuffer = data;
  coll->next.msched = LCIU_malloc(sizeof(LCIXC_mcoll_sched_t[sched_ops]));
  coll->send_comp = LCIXC_coll_comp(ep->cmd_comp_type, ep->device);
  coll->recv_comp = LCIXC_coll_comp(ep->msg_comp_type, ep->device);
  atomic_init(&coll->inprogress, 0);
}

static inline void LCIXC_lcoll_init(LCIX_collective_t coll, LCI_endpoint_t ep,
                                    LCI_tag_t tag, LCI_op_t op, LCI_comp_t comp,
                                    void* user_context, LCI_lbuffer_t data,
                                    size_t sched_ops)
{
  coll->cur = 0;
  coll->total = 0;
  coll->type = LCI_LONG;
  coll->tag = tag;
  coll->ep = ep;
  coll->op = op;
  coll->user_comp = comp;
  coll->user_context = user_context;
  coll->data.lbuffer = data;
  coll->next.lsched = LCIU_malloc(sizeof(LCIXC_lcoll_sched_t[sched_ops]));
  coll->send_comp = LCIXC_coll_comp(ep->cmd_comp_type, ep->device);
  coll->recv_comp = LCIXC_coll_comp(ep->msg_comp_type, ep->device);
  atomic_init(&coll->inprogress, 0);
}

static inline void LCIXC_mcoll_complete(LCI_endpoint_t ep, LCI_mbuffer_t buffer,
                                        LCI_tag_t tag, LCI_comp_t completion,
                                        void* user_context)
{
  LCII_context_t* ctx = LCIU_malloc(sizeof(LCII_context_t));
  LCII_initilize_comp_attr(ctx->comp_attr);
  LCII_comp_attr_set_comp_type(ctx->comp_attr, ep->msg_comp_type);
  ctx->data_type = LCI_MEDIUM;
  ctx->user_context = user_context;
  ctx->data = (LCI_data_t){.mbuffer = buffer};
  ctx->rank = -1; /* this doesn't make much sense for collectives */
  ctx->tag = tag;
  ctx->completion = completion;
  lc_ce_dispatch(ctx);
}

static inline void LCIXC_lcoll_complete(LCI_endpoint_t ep, LCI_lbuffer_t buffer,
                                        LCI_tag_t tag, LCI_comp_t completion,
                                        void* user_context)
{
  LCII_context_t* ctx = LCIU_malloc(sizeof(LCII_context_t));
  LCII_initilize_comp_attr(ctx->comp_attr);
  LCII_comp_attr_set_comp_type(ctx->comp_attr, ep->msg_comp_type);
  ctx->data_type = LCI_LONG;
  ctx->user_context = user_context;
  ctx->data = (LCI_data_t){.lbuffer = buffer};
  ctx->rank = -1; /* this doesn't make much sense for collectives */
  ctx->tag = tag;
  ctx->completion = completion;
  lc_ce_dispatch(ctx);
}

static inline void LCIXC_mcoll_fini(LCIX_collective_t* collp)
{
  LCIX_collective_t coll = *collp;
  LCIXC_coll_comp_free(coll->ep->cmd_comp_type, &coll->send_comp);
  LCIXC_coll_comp_free(coll->ep->msg_comp_type, &coll->recv_comp);
  LCIU_free(coll->next.msched);

  LCIXC_mcoll_complete(coll->ep, coll->data.mbuffer, coll->tag, coll->user_comp,
                       coll->user_context);

  LCIU_free(coll);
  *collp = NULL;
}

static inline void LCIXC_lcoll_fini(LCIX_collective_t* collp)
{
  LCIX_collective_t coll = *collp;
  LCIXC_coll_comp_free(coll->ep->cmd_comp_type, &coll->send_comp);
  LCIXC_coll_comp_free(coll->ep->msg_comp_type, &coll->recv_comp);
  LCIU_free(coll->next.lsched);

  LCIXC_lcoll_complete(coll->ep, coll->data.lbuffer, coll->tag, coll->user_comp,
                       coll->user_context);

  LCIU_free(coll);
  *collp = NULL;
}

static inline void LCIXC_mcoll_sched(LCIX_collective_t coll,
                                     LCIXC_coll_sched_type_t type, int rank,
                                     LCI_mbuffer_t src, LCI_mbuffer_t dst)
{
  LCIXC_mcoll_sched_t* sched = &coll->next.msched[coll->total++];
  sched->type = type;
  sched->rank = rank;
  sched->src = src;
  sched->dst = dst;
}

static inline void LCIXC_lcoll_sched(LCIX_collective_t coll,
                                     LCIXC_coll_sched_type_t type, int rank,
                                     LCI_lbuffer_t src, LCI_lbuffer_t dst)
{
  LCIXC_lcoll_sched_t* sched = &coll->next.lsched[coll->total++];
  sched->type = type;
  sched->rank = rank;
  sched->src = src;
  sched->dst = dst;
}

#endif /* LCIX_COLL_H */
