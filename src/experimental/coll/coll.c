#include "lci.h"
#include "experimental/coll/coll.h"

void LCIXC_coll_handler(LCI_request_t req)
{
  LCIX_collective_t coll = req.user_context;
  atomic_fetch_sub_explicit(&coll->inprogress, 1, memory_order_relaxed);
}

static inline void LCIXC_check_inprogress(LCI_comp_t comp,
                                          LCI_comp_type_t comp_type)
{
  LCI_error_t ret = LCI_ERR_RETRY;
  LCI_request_t req;
  switch (comp_type) {
    case LCI_COMPLETION_QUEUE:
      ret = LCI_queue_pop(comp, &req);
      break;
    case LCI_COMPLETION_SYNC:
      ret = LCI_sync_test(comp, &req);
      break;
  }
  if (LCI_OK == ret) {
    LCIX_collective_t coll = req.user_context;
    atomic_fetch_sub_explicit(&coll->inprogress, 1, memory_order_relaxed);
  }
}

static inline LCI_error_t LCIXC_mcoll_progress(LCIX_collective_t* collp)
{
  LCIX_collective_t coll = *collp;
  LCI_error_t ret = LCI_ERR_RETRY;
  size_t inprogress;
  /* check recv completion for queues and synchronizers */
  LCIXC_check_inprogress(coll->recv_comp, coll->ep->msg_comp_type);

  /* check if operation in progress and bail out early */
  inprogress = atomic_load_explicit(&coll->inprogress, memory_order_relaxed);
  if (inprogress) return ret;

  if (coll->cur < coll->total) {
    /* get next scheduled operation */
    LCIXC_mcoll_sched_t* sched = &coll->next.msched[coll->cur];
    switch (sched->type) {
      case LCIXC_COLL_SEND:
        ret = LCI_sendm(coll->ep, sched->src, sched->rank, coll->tag);
        break;
      case LCIXC_COLL_RECV:
        atomic_store_explicit(&coll->inprogress, 1, memory_order_relaxed);
        ret = LCI_recvm(coll->ep, sched->src, sched->rank, coll->tag,
                        coll->recv_comp, coll);
        break;
      case LCIXC_COLL_SENDRECV:
        atomic_store_explicit(&coll->inprogress, 1, memory_order_relaxed);
        ret = LCI_sendm(coll->ep, sched->src, sched->rank, coll->tag);
        if (LCI_OK != ret) break;
        ret = LCI_recvm(coll->ep, sched->dst, sched->rank, coll->tag,
                        coll->recv_comp, coll);
        break;
      case LCIXC_COLL_OP:
        coll->op(sched->dst.address, sched->src.address, sched->dst.length);
        ret = LCI_OK;
        break;
      case LCIXC_COLL_MEM:
        memmove(sched->dst.address, sched->src.address, sched->dst.length);
        ret = LCI_OK;
        break;
      case LCIXC_COLL_FREE:
        LCI_mbuffer_free(sched->src);
        ret = LCI_OK;
        break;
    }

    if (LCI_OK == ret) {
      /* operation successful, move to next one */
      coll->cur++;
    } else {
      /* operation failed, stay at current one and reset inprogress */
      atomic_store_explicit(&coll->inprogress, 0, memory_order_relaxed);
    }
  } else {
    /* we're done, set user completion and free collective resources */
    LCIXC_mcoll_fini(collp);
  }
  return ret;
}

static inline LCI_error_t LCIXC_lcoll_progress(LCIX_collective_t* collp)
{
  LCIX_collective_t coll = *collp;
  LCI_error_t ret = LCI_ERR_RETRY;
  size_t inprogress;
  /* check send and recv completion for queues and synchronizers */
  LCIXC_check_inprogress(coll->send_comp, coll->ep->cmd_comp_type);
  LCIXC_check_inprogress(coll->recv_comp, coll->ep->msg_comp_type);

  /* check if operation in progress and bail out early */
  inprogress = atomic_load_explicit(&coll->inprogress, memory_order_relaxed);
  if (inprogress) return ret;

  if (coll->cur < coll->total) {
    /* get next scheduled operation */
    LCIXC_lcoll_sched_t* sched = &coll->next.lsched[coll->cur];
    switch (sched->type) {
      case LCIXC_COLL_SEND:
        atomic_store_explicit(&coll->inprogress, 1, memory_order_relaxed);
        ret = LCI_sendl(coll->ep, sched->src, sched->rank, coll->tag,
                        coll->send_comp, coll);
        break;
      case LCIXC_COLL_RECV:
        atomic_store_explicit(&coll->inprogress, 1, memory_order_relaxed);
        ret = LCI_recvl(coll->ep, sched->src, sched->rank, coll->tag,
                        coll->recv_comp, coll);
        break;
      case LCIXC_COLL_SENDRECV:
        atomic_store_explicit(&coll->inprogress, 2, memory_order_relaxed);
        ret = LCI_sendl(coll->ep, sched->src, sched->rank, coll->tag,
                        coll->send_comp, coll);
        if (LCI_OK != ret) break;
        ret = LCI_recvl(coll->ep, sched->dst, sched->rank, coll->tag,
                        coll->recv_comp, coll);
        break;
      case LCIXC_COLL_OP:
        coll->op(sched->dst.address, sched->src.address, sched->dst.length);
        ret = LCI_OK;
        break;
      case LCIXC_COLL_MEM:
        memmove(sched->dst.address, sched->src.address, sched->dst.length);
        ret = LCI_OK;
        break;
      case LCIXC_COLL_FREE:
        LCI_lbuffer_free(sched->src);
        ret = LCI_OK;
        break;
    }

    if (LCI_OK == ret) {
      /* operation successful, move to next one */
      coll->cur++;
    } else {
      /* operation failed, stay at current one and reset inprogress */
      atomic_store_explicit(&coll->inprogress, 0, memory_order_relaxed);
    }
  } else {
    /* we're done, set user completion and free collective resources */
    LCIXC_lcoll_fini(collp);
  }
  return ret;
}

LCI_error_t LCIX_coll_progress(LCIX_collective_t* collp)
{
  /* there isn't any progress to do on an invalid or complete collective */
  if (!collp) return LCI_OK;

  LCIX_collective_t coll = *collp;
  switch (coll->type) {
    case LCI_MEDIUM:
      return LCIXC_mcoll_progress(collp);
    case LCI_LONG:
      return LCIXC_lcoll_progress(collp);
  }

  /* type must be medium or long for now */
  return LCI_ERR_FEATURE_NA;
}
