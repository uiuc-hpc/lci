#include "runtime/lcii.h"

LCI_error_t LCI_plist_create(LCI_plist_t* plist_ptr)
{
  struct LCI_plist_s* plist = LCIU_malloc(sizeof(struct LCI_plist_s));
  plist->match_type = LCI_MATCH_RANKTAG;
  plist->cmd_comp_type = LCI_COMPLETION_QUEUE;
  plist->msg_comp_type = LCI_COMPLETION_QUEUE;
  plist->default_comp = LCI_UR_CQ;
  *plist_ptr = plist;
  return LCI_OK;
}

LCI_error_t LCI_plist_free(LCI_plist_t* plist)
{
  LCIU_free(*plist);
  *plist = NULL;
  return LCI_OK;
}

LCI_error_t LCI_plist_get(LCI_endpoint_t ep, LCI_plist_t* plist_ptr)
{
  struct LCI_plist_s* plist = LCIU_malloc(sizeof(struct LCI_plist_s));
  plist->match_type = ep->match_type;
  plist->cmd_comp_type = ep->cmd_comp_type;
  plist->msg_comp_type = ep->msg_comp_type;
  plist->default_comp = ep->default_comp;
  *plist_ptr = plist;
  return LCI_OK;
}

LCI_error_t LCI_plist_decode(LCI_plist_t plist, char* string)
{
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_plist_set_match_type(LCI_plist_t plist, LCI_match_t match_type)
{
  plist->match_type = match_type;
  return LCI_OK;
}

/**
 * Set completion type
 * @param plist the property list to set
 * @param port specify command port or message port to set
 * @param comp_type specify the completion type
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of
 * failure.
 */
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
      LCM_DBG_Assert(false, "unknown port!\n");
  }
  return LCI_OK;
}

LCI_error_t LCI_plist_set_default_comp(LCI_plist_t plist, LCI_comp_t comp)
{
  plist->default_comp = comp;
  return LCI_OK;
}
