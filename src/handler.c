#include "lcii.h"

LCI_error_t LCI_handler_create(LCI_device_t device, LCI_handler_t handler,
                               LCI_comp_t* completion)
{
  *completion = handler;
  return LCI_OK;
}