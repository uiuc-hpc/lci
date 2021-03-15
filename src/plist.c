#include "lci.h"
#include "lcii.h"

LCI_error_t LCI_plist_create(LCI_plist_t *prop_ptr)
{
  struct LCI_plist_s* prop = LCIU_malloc(sizeof(struct LCI_plist_s));
  prop->ctype = LCI_COMM_COLLECTIVE;
  prop->mtype = LCI_MSG_LONG;
  prop->rtype = LCI_COMPLETION_ONE2ONEL;
  prop->ltype = LCI_COMPLETION_ONE2ONEL;

  *prop_ptr = prop;
  return LCI_OK;
}

LCI_error_t LCI_plist_free(LCI_plist_t *plist)
{
  LCIU_free(*plist);
  return LCI_OK;
}

LCI_error_t LCI_plist_get(LCI_endpoint_t endpoint, LCI_plist_t *plist) {
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_plist_decode(LCI_plist_t plist, char *string) {
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_plist_set_CQ(LCI_plist_t plist, LCI_comp_t* cq)
{
  plist->cq = *cq;
  return LCI_OK;
}

LCI_error_t LCI_plist_set_comm_type(LCI_plist_t plist, LCI_comm_t type)
{
  plist->ctype = type;
  return LCI_OK;
}

LCI_error_t LCI_plist_set_match_type(LCI_plist_t plist, LCI_match_t type)
{
  plist->match_type = type;
  return LCI_OK;
}

LCI_error_t LCI_plist_set_msg_type(LCI_plist_t plist, LCI_msg_type_t type)
{
  plist->mtype = type;
  return LCI_OK;
}

LCI_error_t LCI_plist_set_MT(LCI_plist_t plist, LCI_MT_t* mt)
{
  plist->mt = *mt;
  return LCI_OK;
}

LCI_error_t LCI_plist_set_handler(LCI_plist_t plist, LCI_handler_t* handler)
{
  plist->handler = handler;
  return LCI_OK;
}

LCI_error_t LCI_plist_set_dynamic(LCI_plist_t	plist, LCI_port_t port, LCI_dynamic_t type)
{
  if (port == LCI_PORT_COMMAND)
    plist->cdtype = type;
  else
    plist->mdtype = type;
  return LCI_OK;
}

LCI_error_t LCI_plist_set_allocator(LCI_plist_t plist, LCI_allocator_t allocator)
{
  plist->allocator = allocator;
  return LCI_OK;
}

LCI_error_t LCI_plist_set_completion(LCI_plist_t plist, LCI_port_t port,
                                  LCI_comptype_t type)
{
  if (port == LCI_PORT_COMMAND)
    plist->ltype = type;
  else
    plist->rtype = type;
  return LCI_OK;
}


