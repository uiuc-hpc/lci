#ifndef LCI_LCII_RCACHE_H
#define LCI_LCII_RCACHE_H
#include "runtime/lcii.h"

typedef void* LCII_rcache_t;

LCI_error_t LCII_rcache_init(LCI_device_t device);
void LCII_rcache_fina(LCI_device_t device);
void LCII_rcache_reg(LCI_device_t device, void* address, size_t length,
                     LCI_segment_t segment);
LCI_error_t LCII_rcache_dereg(LCI_segment_t segment);

#endif  // LCI_LCII_RCACHE_H
