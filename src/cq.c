#include "lc.h"

#include "lc_priv.h"
#include "lc/pool.h"

lc_status lc_cq_popval(lc_ep ep, lc_req* req)
{
  lc_packet* p = cq_pop(&ep->cq);
  if (!p) return LC_ERR_RETRY;
  memcpy(req, p->context.req, sizeof(struct lc_req));
  lc_pool_put(ep->dev->pkpool, p);
  return LC_OK;
}

lc_status lc_cq_popref(lc_ep ep, lc_req** req)
{
  lc_packet* p = cq_pop(&ep->cq);
  if (!p) return LC_ERR_RETRY;
  *req = p->context.req;
  return LC_OK;
}

lc_status lc_cq_reqfree(lc_ep ep, lc_req* req)
{
  lc_pool_put(ep->dev->pkpool, req->parent);
  return LC_OK;
}
