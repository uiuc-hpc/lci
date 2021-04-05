#include "lci.h"
#include "lcii.h"

LCI_error_t LCI_memory_register(LCI_device_t device, void *address, size_t length,
                                LCI_segment_t *segment) {
  LCI_segment_t mr = (LCI_segment_t) LCIU_malloc(sizeof(struct LCI_segment_s));
  *segment = mr;
  lc_server* dev = LCI_DEVICES[device];
  uintptr_t rma_mem = lc_server_rma_reg(dev, address, length);
  mr->mr_p = rma_mem;
  mr->address = address;
  mr->length = length;
  return LCI_OK;
}

LCI_error_t LCI_memory_deregister(LCI_segment_t* segment)
{
  lc_server_rma_dereg((*segment)->mr_p);
  *segment = NULL;
  return LCI_OK;
}

LCI_error_t LCI_malloc(size_t size, LCI_segment_t segment, void **address) {
  return LCI_ERR_FEATURE_NA;
}

LCI_error_t LCI_free(LCI_segment_t segment, void *address) {
  return LCI_ERR_FEATURE_NA;
}