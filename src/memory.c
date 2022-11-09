#include "lci.h"
#include "lcii.h"

/** @defgroup LCIMemory LCI memory management
 * This group is for LCI memory and buffer management.
 */

/**
 * @ingroup LCIMemory
 * @brief Register a memory segment to a device.
 * @param[in]  device  the device to register to
 * @param[in]  address the starting address of the registered memory segment
 * @param[in]  length  the size in bytes of the registered memory segment
 * @param[out] segment a descriptor to the segment
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of
 * failure.
 */
LCI_error_t LCI_memory_register(LCI_device_t device, void* address,
                                size_t length, LCI_segment_t* segment)
{
  LCI_segment_t mr = (LCI_segment_t)LCIU_malloc(sizeof(struct LCIS_mr_t));
  *mr = LCIS_rma_reg(device->server, address, length);
  *segment = mr;
  return LCI_OK;
}

/**
 * @ingroup LCIMemory
 * @brief Deregister a memory region from a device.
 * @param[in] device  the device to deregister from
 * @param[io] segment a descriptor to the segment to be deregistered, it
 *                will be set to NULL.
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of
 * failure.
 */
LCI_error_t LCI_memory_deregister(LCI_segment_t* segment)
{
  LCM_DBG_Assert(*segment != NULL, "*segment is NULL\n");
  LCIS_rma_dereg(**segment);
  LCIU_free(*segment);
  *segment = NULL;
  return LCI_OK;
}

/**
 * @ingroup LCIMemory
 * @brief requests a medium buffer for communication using the specified device.
 * @param[in] device device id
 * @param[out] mbuffer  medium buffer to be allocated
 * @return Error code
 */
LCI_error_t LCI_mbuffer_alloc(LCI_device_t device, LCI_mbuffer_t* mbuffer)
{
  lc_packet* packet = lc_pool_get_nb(device->pkpool);
  if (packet == NULL)
    // no packet is available
    return LCI_ERR_RETRY;
  packet->context.poolid = -1;

  mbuffer->address = packet->data.address;
  mbuffer->length = LCI_MEDIUM_SIZE;
  return LCI_OK;
}

/**
 * @ingroup LCIMemory
 * @brief free a medium buffer.
 * @param[in] mbuffer  medium buffer to be freed
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of
 * failure.
 */
LCI_error_t LCI_mbuffer_free(LCI_mbuffer_t mbuffer)
{
  lc_packet* packet = LCII_mbuffer2packet(mbuffer);
  LCII_free_packet(packet);
  return LCI_OK;
}

/**
 * @ingroup LCIMemory
 * @brief requests a long buffer for communication using the specified device.
 * @param[in] device device id
 * @param[in] size      desired size of the long buffer to be allocated
 * @param[out] lbuffer  long buffer to be allocated
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of
 * failure.
 */
LCI_error_t LCI_lbuffer_alloc(LCI_device_t device, size_t size,
                              LCI_lbuffer_t* lbuffer)
{
  lbuffer->length = size;
  lbuffer->address = LCIU_malloc(size);
  LCI_memory_register(device, lbuffer->address, lbuffer->length,
                      &lbuffer->segment);
  if (LCI_TOUCH_LBUFFER) {
    char* p = (char*)lbuffer->address;
    if ((uint64_t)p % LCI_PAGESIZE != 0) *(char*)p = 'a';
    p = (char*)((uintptr_t)(p + LCI_PAGESIZE - 1) / LCI_PAGESIZE *
                LCI_PAGESIZE);
    for (; p < (char*)lbuffer->address + size; p += LCI_PAGESIZE) {
      *(char*)p = 'a';
    }
  }
  return LCI_OK;
}

LCI_error_t LCI_lbuffer_memalign(LCI_device_t device, size_t size,
                                 size_t alignment, LCI_lbuffer_t* lbuffer)
{
  lbuffer->length = size;
  lbuffer->address = LCIU_memalign(alignment, size);
  LCI_memory_register(device, lbuffer->address, lbuffer->length,
                      &lbuffer->segment);
  return LCI_OK;
}

/**
 * @ingroup LCIMemory
 * @brief free a long buffer.
 * @param[in] lbuffer long buffer to be freed
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of
 * failure.
 */
LCI_error_t LCI_lbuffer_free(LCI_lbuffer_t lbuffer)
{
  LCI_memory_deregister(&lbuffer.segment);
  LCIU_free(lbuffer.address);
  return LCI_OK;
}