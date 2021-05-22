#include "lci.h"
#include "lcii.h"

LCI_error_t LCI_puts(LCI_endpoint_t ep, LCI_short_t src, int rank,
                     LCI_tag_t tag, uintptr_t remote_completion)
{
  LCI_DBG_Assert(remote_completion == LCI_UR_CQ_REMOTE, "Only support default remote completion (dynamic put to remote LCI_UR_CQ)\n");
  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_sends(ep->server, rep->handle, &src, sizeof(LCI_short_t),
                  LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_SHORT, tag));
  return LCI_OK;
}

LCI_error_t LCI_putm(LCI_endpoint_t ep, LCI_mbuffer_t mbuffer, int rank,
                     LCI_tag_t tag, LCI_lbuffer_t remote_buffer,
                     uintptr_t remote_completion)
{ 
//  LC_POOL_GET_OR_RETN(ep->pkpool, p);
//  lc_pk_init(ep, (size > 1024) ? lc_pool_get_local(ep->pkpool) : -1, LC_PROTO_DATA, p);
//  struct lc_rep* rep = &(ep->rep[rank]);
//  memcpy(p->data.buffer, src, size);
//  lc_server_put(ep->server, rep->handle, rep->base, offset, rep->rkey, size, LCII_MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta), p);
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_putma(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                     LCI_tag_t tag, uintptr_t remote_completion) {
  LCI_DBG_Assert(remote_completion == LCI_UR_CQ_REMOTE, "Only support default remote completion (dynamic put to remote LCI_UR_CQ)\n");
  lc_packet* packet = lc_pool_get_nb(ep->pkpool);
  if (packet == NULL)
    // no packet is available
    return LCI_ERR_RETRY;
  packet->context.poolid = (buffer.length > LCI_PACKET_RETURN_THRESHOLD) ?
                           lc_pool_get_local(ep->pkpool) : -1;
  memcpy(packet->data.address, buffer.address, buffer.length);

  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->data.mbuffer.address = (void*) packet->data.address;
  ctx->msg_type = LCI_MSG_RDMA_MEDIUM;

  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_send(ep->server, rep->handle, packet->data.address, buffer.length,
                 ep->server->heap_mr,
                 LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDMA_MEDIUM, tag), ctx);
  return LCI_OK;
}

LCI_error_t LCI_putb(LCI_mbuffer_t buffer, size_t size, int rank, uint16_t meta, LCI_endpoint_t ep, void* sync)
{ 
//  lc_packet* p = LCII_mbuffer2packet(buffer);
//  lc_pk_init(ep, (size > 1024) ? lc_pool_get_local(ep->pkpool) : -1, LC_PROTO_DATA, p);
//  p->context.ref = USER_MANAGED;
//  p->context.sync = sync;
//  struct lc_rep* rep = &(ep->rep[rank]);
//  lc_server_send(ep->server, rep->handle, size, p, LCII_MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta));
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_putl(LCI_endpoint_t ep, LCI_lbuffer_t local_buffer,
                     LCI_comp_t local_completion, int rank, LCI_tag_t tag,
                     LCI_lbuffer_t remote_buffer, uintptr_t remote_completion)
{
  return LCI_ERR_FEATURE_NA;
}
