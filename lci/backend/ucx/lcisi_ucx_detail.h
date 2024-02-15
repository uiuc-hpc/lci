#ifndef LCI_LCISI_UCX_DETAIL_H
#define LCI_LCISI_UCX_DETAIL_H

#include <ucp/api/ucp.h>

// Borrowed from UCX library
static ucs_status_t LCISI_wait_status_ptr(ucp_worker_h worker,
                                          ucs_status_ptr_t status_ptr)
{
  ucs_status_t status;

  if (status_ptr == NULL) {
    status = UCS_OK;
  } else if (UCS_PTR_IS_PTR(status_ptr)) {
    do {
      ucp_worker_progress(worker);
      status = ucp_request_test(status_ptr, NULL);
    } while (status == UCS_INPROGRESS);
    ucp_request_release(status_ptr);
  } else {
    status = UCS_PTR_STATUS(status_ptr);
  }

  return status;
}

#endif  // LCI_LCISI_UCX_DETAIL_H
