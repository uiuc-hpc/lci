#include "lc.h"
#include "lc_priv.h"

lc_status lc_send(void* src, size_t size, int rank, int tag, lc_ep ep, lc_send_cb cb, void* ce)
{
  int ret;
  if (size <= LC_MAX_INLINE) {
    ret = lc_sends(src, size, rank, tag, ep);
    if (ret == LC_OK)
      cb(ce);
    return ret;
  } else if (size <= PACKET_DATA_SIZE) {
    ret = lc_sendm(src, size, rank, tag, ep);
    if (ret == LC_OK)
      cb(ce);
    return ret;
  } else {
    return lc_sendl(src, size, rank, tag, ep, cb, ce);
  }
}

lc_status lc_recv(void* src, size_t size, int rank, int tag, lc_ep ep, lc_req* req)
{
  if (size <= PACKET_DATA_SIZE) {
    return lc_recvm(src, size, rank, tag, ep, req);
  } else {
    return lc_recvl(src, size, rank, tag, ep, req);
  }
}
