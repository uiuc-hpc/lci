#include "lci.h"
#include "lci_priv.h"
#include "pool.h"

LCI_error_t LCI_puti(void* src, size_t size, int rank, int rma_id, int offset, LCI_endpoint_t ep)  
{ 
  struct lc_rep* rep = &(ep->rep[rank]);  
  assert(rma_id == 0 && "fixme"); 
  lc_server_puts(ep->server, rep->handle, src, rep->base, offset, rep->rkey, size);  
  return LCI_OK;  
}

LCI_error_t LCI_putbc(void* src, size_t size, int rank, int rma_id, int offset, uint16_t meta, LCI_endpoint_t ep)  
{ 
  LC_POOL_GET_OR_RETN(ep->pkpool, p);
  lc_pk_init(ep, (size > 1024) ? lc_pool_get_local(ep->pkpool) : -1, LC_PROTO_DATA, p);
  struct lc_rep* rep = &(ep->rep[rank]);
  memcpy(&p->data, src, size);
  lc_server_putms(ep->server, rep->handle, rep->base, offset, rep->rkey, size, MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta), p);
  return LCI_OK;
}

LCI_error_t LCI_putb(LCI_bdata_t buffer, size_t size, int rank, uint16_t meta, LCI_endpoint_t ep, void* sync)  
{ 
  lc_packet* p = (lc_packet*) buffer;
  lc_pk_init(ep, (size > 1024) ? lc_pool_get_local(ep->pkpool) : -1, LC_PROTO_DATA, p);
  p->context.ref = USER_MANAGED;
  p->context.sync = sync;
  struct lc_rep* rep = &(ep->rep[rank]);
  lc_server_sendm(ep->server, rep->handle, size, p, MAKE_PROTO(ep->gid, LC_PROTO_LONG, meta));
  return LCI_OK;
}
