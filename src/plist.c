#include "lci.h"
#include "src/include/lci_priv.h"

LCI_error_t LCI_PL_create(LCI_PL_t* prop_ptr)
{
  struct LCI_PL_s* prop = 0;
  posix_memalign((void**) &prop, 64, sizeof(struct LCI_PL_s));
  prop->ctype = LCI_COMM_COLLECTIVE;
  prop->mtype = LCI_MSG_DIRECT;
  prop->rtype = LCI_COMPLETION_ONE2ONEL;
  prop->ltype = LCI_COMPLETION_ONE2ONEL;

  *prop_ptr = prop;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_cq(LCI_CQ_t* cq, LCI_PL_t* prop)
{
  (*prop)->cq = *cq;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_comm_type(LCI_comm_t type, LCI_PL_t* prop)
{
  (*prop)->ctype = type;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_msg_type(LCI_msg_t type, LCI_PL_t* prop)
{
  (*prop)->mtype = type;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_mt(LCI_MT_t* mt, LCI_PL_t* prop)
{
  (*prop)->mt = *mt;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_handler(LCI_Handler handler, LCI_PL_t* prop)
{
  (*prop)->handler = handler;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_allocator(LCI_Allocator handler, LCI_PL_t* prop)
{
  (*prop)->allocator = handler;
  return LCI_OK;
}

LCI_API
LCI_error_t LCI_PL_set_completion(LCI_port_t port, LCI_comp_t type, LCI_PL_t* prop)
{
  if (port == LCI_PORT_COMMAND)
    (*prop)->ltype = type;
  else
    (*prop)->rtype = type;
  return LCI_OK;
}


