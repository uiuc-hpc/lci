#include "lc.h"

#include "lc_priv.h"
#include "lc/pool.h"

#include <assert.h>

#define MAX_EP 256

size_t server_max_inline = SERVER_MAX_INLINE;
const size_t server_max_recvs = SERVER_MAX_RCVS;

struct lci_dev* dev;
int lcg_size;
int lcg_rank;
int lcg_ndev;
char lcg_name[256];

int lc_current_id = 0;
__thread int lc_core_id = -1;
int server_deadlock_alert;
lc_ep lcg_ep_list[MAX_EP];
int lcg_nep = 0;

lc_status lc_init() 
{
  lc_pm_master_init(&lcg_size, &lcg_rank, lcg_name);
  return LC_OK;
}

lc_status lc_dev_open(lc_dev* dev)
{
  posix_memalign((void**) dev, 4096, sizeof(struct lci_dev));
  lci_dev_init(*dev);
  return LC_OK;
}

lc_status lc_ep_open(lc_dev dev, long cap, lc_ep* ep)
{
  lci_ep_open(dev, ep, cap);
  return LC_OK;
}

lc_status lc_ep_set_alloc(lc_ep ep, lc_alloc_fn alloc, lc_free_fn free, void* ctx)
{
  ep->alloc = alloc;
  ep->free = free;
  ep->ctx = ctx;
  return LC_OK;
}

lc_status lc_finalize()
{
  return LC_OK;
}

void lc_get_proc_num(int *rank)
{
  *rank = lcg_rank;
}

void lc_get_num_proc(int *size)
{
  *size = lcg_size;
}

LC_INLINE
lc_status lci_send_qalloc(struct lci_ep *ep, lc_rep tep, void* src, size_t size, lc_meta meta, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->dev->pkpool, p);
  p->context.ep = ep;
  p->context.poolid = (size > 128) ? lc_pool_get_local(ep->dev->pkpool) : -1;

  if (size <= (int) SHORT_MSG_SIZE) {
    lc_server_send(ep->dev->handle, ep, tep->handle, src, size, p, MAKE_PROTO(tep->eid, LC_PROTO_DATA, meta.val));
    req->flag = 1;
  } else {
    INIT_CTX(req);
    p->data.rts.req = (uintptr_t) req;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    p->data.rts.reid = ep->eid;
    lc_server_send(ep->dev->handle, ep, tep->handle, &p->data, sizeof(struct packet_rts), p, MAKE_PROTO(tep->eid, LC_PROTO_RTS, meta.val));
  }
  return LC_OK;
}

LC_INLINE
lc_status lci_send_qshort(struct lci_ep *ep, lc_rep tep, void* src, size_t size, lc_meta meta, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->dev->pkpool, p);
  p->context.ep = ep;
  p->context.poolid = (size > 128) ? lc_pool_get_local(ep->dev->pkpool) : -1;

  if (size <= (int) SHORT_MSG_SIZE) {
    lc_server_send(ep->dev->handle, ep, tep->handle, src, size, p, MAKE_PROTO(tep->eid, LC_PROTO_DATA, meta.val));
    req->flag = 1;
  } else {
    assert(0 && "Only short data");
  }
  return LC_OK;
}

LC_INLINE
lc_status lci_send_tag(struct lci_ep *ep, lc_rep rep, void* src, size_t size, lc_meta tag, lc_req* req)
{
  LC_POOL_GET_OR_RETN(ep->dev->pkpool, p);
  p->context.ep = ep;
  p->context.poolid = (size > 64) ? lc_pool_get_local(ep->dev->pkpool) : -1;

  if (size <= (int) SHORT_MSG_SIZE) {
    lc_server_send(ep->dev->handle, ep, rep->handle, src, size, p,
                   MAKE_PROTO(rep->eid, LC_PROTO_DATA, tag.val));
    req->flag = 1;
  } else {
    INIT_CTX(req);
    p->data.rts.req = (uintptr_t) req;
    p->data.rts.src_addr = (uintptr_t) src;
    p->data.rts.size = size;
    p->data.rts.reid = ep->eid;
    lc_server_send(ep->dev->handle, ep, rep->handle, &p->data,
                   sizeof(struct packet_rts), p,
                   MAKE_PROTO(rep->eid, LC_PROTO_RTS, tag.val));
  }
  return LC_OK;
}

LC_INLINE
lc_status lci_recv_tag(struct lci_ep *ep, lc_rep rep, void* src, size_t size, lc_meta tag, lc_req* req)
{
  INIT_CTX(req);
  req->rhandle = rep->handle;
  lc_key key = lc_make_key(rep->rank, tag.val);
  lc_value value = (lc_value)req;
  if (!lc_hash_insert(ep->tbl, key, &value, CLIENT)) {
    lc_packet* p = (lc_packet*) value;
    if (p->context.proto == LC_PROTO_DATA) {
      memcpy(src, p->data.buffer, p->context.req->size);
      req->size = p->context.req->size;
      req->flag = 1;
      lc_pool_put(ep->dev->pkpool, p);
    } else {
      p->context.ep = ep;
      p->context.req = req;
      p->context.proto = LC_PROTO_RTR;
      req->size = p->data.rts.size;
      lci_rdz_prepare(ep, src, size, p);
      lc_server_send(ep->dev->handle, ep, rep->handle, &p->data, sizeof(struct packet_rtr), p,
                     LC_PROTO_RTR);
    }
  }
  return LC_OK;
}

LC_INLINE
lc_status lci_produce(struct lci_ep* ep, lc_wr* wr, lc_req* req)
{
  lc_data* source = &wr->source_data;
  lc_data* target = &wr->target_data;
  // FIXME(danghvu): how do we know gid vs. eid vs. veid.

  // TODO(danghvu): handle more type of source data.
  void* src_buf = source->addr;
  size_t size = source->size;

  switch(target->type) {
    case DAT_EXPL :
      return lci_send_tag(ep, wr->target, src_buf, size, wr->meta, req);
    case DAT_ALLOC :
      return lci_send_qalloc(ep, wr->target, src_buf, size, wr->meta, req);
    case DAT_PIGGY :
      return lci_send_qshort(ep, wr->target, src_buf, size, wr->meta, req);
    default:
      assert(0 && "Invalid type");
  }
}

LC_INLINE
lc_status lci_consume(struct lci_ep* ep, lc_wr* wr, lc_req* req)
{
  lc_data* source = &wr->source_data;
  lc_data* target = &wr->target_data;
  void* tgt_buf = target->addr;
  size_t size = target->size;

  switch(source->type) {
    case DAT_EXPL :
      return lci_recv_tag(ep, wr->target, tgt_buf, size, wr->meta, req);
    default:
      assert(0 && "Invalid type");
  }

  return LC_OK;
}

lc_status lc_send_tag(lc_ep ep, lc_rep rep, void* src, size_t size, lc_meta tag, lc_req* req)
{
  return lci_send_tag(ep, rep, src, size, tag, req);
}

lc_status lc_recv_tag(lc_ep ep, lc_rep rep, void* src, size_t size, lc_meta tag, lc_req* req)
{
  return lci_recv_tag(ep, rep, src, size, tag, req);
}

lc_status lc_send_qalloc(lc_ep ep, lc_rep rep, void* src, size_t size, lc_meta meta, lc_req* req)
{
  return lci_send_qalloc(ep, rep, src, size, meta, req);
}

lc_status lc_recv_qalloc(lc_ep ep, lc_req* req)
{
  lc_packet* p = cq_pop(&ep->cq);
  if (!p) return LC_ERR_RETRY;
  memcpy(req, p->context.req, sizeof(struct lc_req));
  lc_pool_put(ep->dev->pkpool, p);
  return LC_OK;
}

lc_status lc_send_qshort(struct lci_ep *ep, lc_rep tep, void* src, size_t size, lc_meta meta, lc_req* req)
{
  return lci_send_qshort(ep, tep, src, size, meta, req);
}

lc_status lc_recv_qshort(lc_ep ep, lc_req** req)
{
  lc_packet* p = cq_pop(&ep->cq);
  if (!p) return LC_ERR_RETRY;
  *req = p->context.req;
  return LC_OK;
}

lc_status lc_submit(lc_ep ep, lc_wr* wr, lc_req* req)
{
  if (wr->type == WR_PROD) {
    return lci_produce(ep, wr, req);
  } else if (wr->type == WR_CONS) {
    return lci_consume(ep, wr, req);
  }
  return LC_OK;
}

lc_status lc_req_free(lc_ep ep, lc_req* req)
{
  lc_pool_put(ep->dev->pkpool, req->parent);
  return LC_OK;
}

lc_status lc_progress_t(lc_dev dev) // TODO: make a version with index.
{
  lc_server_progress(dev->handle, EP_TYPE_TAG);
  return LC_OK;
}

lc_status lc_progress_q(lc_dev dev) // TODO: make a version with index.
{
  lc_server_progress(dev->handle, EP_TYPE_QUEUE);
  return LC_OK;
}

lc_status lc_progress_sq(lc_dev dev) // TODO: make a version with index.
{
  lc_server_progress(dev->handle, EP_TYPE_SQUEUE);
  return LC_OK;
}

lc_status lc_progress(lc_dev dev)
{
  lc_server_progress(dev->handle, dev->cap);
  return LC_OK;
}

lc_status lc_ep_query(lc_dev dev, int prank, int erank, lc_rep* rep)
{
  lci_ep_connect(dev, prank, erank, rep);
  return LC_OK;
}

lc_status lc_free(lc_ep ep, void* buf)
{
  ep->free(ep->ctx, buf);
  return LC_OK;
}

void lc_pm_barrier() { PMI_Barrier(); }
