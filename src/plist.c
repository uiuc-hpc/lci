#include "lci.h"
#include "lci_priv.h"

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

LCI_error_t LCI_PL_free(LCI_PL_t plist)
{
  free(plist);
  return LCI_OK;
}

LCI_error_t LCI_PL_get(LCI_endpoint_t endpoint, LCI_PL_t plist) {
  LCI_Log(LCI_LOG_WARN, "Not implemented yet!");
  return LCI_OK;
}

LCI_error_t LCI_PL_set_CQ(LCI_PL_t plist, LCI_comp_t* cq)
{
  plist->cq = *cq;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_comm_type(LCI_PL_t plist, LCI_comm_t type)
{
  plist->ctype = type;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_match_type(LCI_PL_t plist, LCI_match_t type)
{
  plist->match_type = type;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_msg_type(LCI_PL_t plist, LCI_msg_t type)
{
  plist->mtype = type;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_MT(LCI_PL_t plist, LCI_MT_t* mt)
{
  plist->mt = *mt;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_handler(LCI_PL_t plist, LCI_handler_t* handler)
{
  plist->handler = handler;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_dynamic(LCI_PL_t	plist, LCI_port_t port, LCI_dynamic_t type)
{
  if (port == LCI_PORT_COMMAND)
    plist->cdtype = type;
  else
    plist->mdtype = type;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_allocator(LCI_PL_t plist, LCI_allocator_t allocator)
{
  plist->allocator = allocator;
  return LCI_OK;
}

LCI_error_t LCI_PL_set_completion(LCI_PL_t plist, LCI_port_t port,
                                  LCI_comptype_t type)
{
  if (port == LCI_PORT_COMMAND)
    plist->ltype = type;
  else
    plist->rtype = type;
  return LCI_OK;
}


