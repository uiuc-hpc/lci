#include "lcii.h"

LCI_plist_t* LCI_PLISTS;

LCI_error_t LCI_plist_create(LCI_plist_t* plist_ptr)
{
  struct LCI_plist_s* plist = LCIU_malloc(sizeof(struct LCI_plist_s));
  plist->match_type = LCI_MATCH_TAG;
  plist->cmd_comp_type = LCI_COMPLETION_QUEUE;
  plist->msg_comp_type = LCI_COMPLETION_QUEUE;
  plist->allocator.malloc = NULL;
  plist->allocator.free = NULL;

  *plist_ptr = plist;
  return LCI_OK;
}

LCI_error_t LCI_plist_free(LCI_plist_t *plist)
{
  LCIU_free(*plist);
  *plist = NULL;
  return LCI_OK;
}

LCI_error_t LCI_plist_get(LCI_endpoint_t ep, LCI_plist_t *plist_ptr)
{
  struct LCI_plist_s* plist = LCIU_malloc(sizeof(struct LCI_plist_s));
  plist->match_type = ep->match_type;
  plist->cmd_comp_type = ep->cmd_comp_type;
  plist->msg_comp_type = ep->msg_comp_type;
  plist->allocator = ep->allocator;
  *plist_ptr = plist;
  return LCI_OK;
}

LCI_error_t LCI_plist_decode(LCI_plist_t plist, char *string)
{
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_plist_set_match_type(LCI_plist_t plist,
                                     LCI_match_t match_type)
{
  plist->match_type = match_type;
  return LCI_OK;
}

LCI_error_t LCI_plist_set_comp_type(LCI_plist_t plist, LCI_port_t port,
                                    LCI_comp_type_t comp_type)
{
  switch (port) {
    case LCI_PORT_COMMAND:
      plist->cmd_comp_type = comp_type;
      break;
    case LCI_PORT_MESSAGE:
      plist->msg_comp_type = comp_type;
      break;
    default:
      LCM_DBG_Assert(false, "unknown port!");
  }
  return LCI_OK;
}

LCI_error_t LCI_plist_set_allocator(LCI_plist_t plist,
                                    LCI_allocator_t allocator)
{
  plist->allocator = allocator;
  return LCI_OK;
}

