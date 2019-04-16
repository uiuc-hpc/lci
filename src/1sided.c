#include "lci.h"
#include "lci_priv.h"
#include "lc/pool.h"

LCI_error_t LCI_puti(void* src, size_t size, int rank, int rma_id, int offset, LCI_endpoint_t ep)  
{ 
  struct lc_rep* rep = &(ep->rep[rank]);  
  assert(rma_id == 0 && "fixme"); 
  lc_server_puts(ep->server, rep->handle, src, rep->base, offset, rep->rkey, size);  
  return LCI_OK;  
}
