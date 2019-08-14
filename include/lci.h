/**
 * @file lci.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 * @brief Header file for all LCI code.
 */

#ifndef LCI_H_
#define LCI_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCI_API __attribute__((visibility("default")))

/**
 * \defgroup LCIGlobal LCI Global variables
 * @{
 */

/**
 * LCI global variables.
 */
int LCI_IMMEDIATE_LENGTH;
int LCI_BUFFERED_LENGTH;
int LCI_NUM_DEVICES;
int LCI_NUM_ENDPOINTS;
int LCI_NUM_PROCESSES;
int LCI_RANK;
int LCI_REGISTERED_MEMORY_SIZE;

/**@}*/

/**
 * \defgroup LCITypes LCI Data Types.
 * @{
 */

/**
 * LCI Status type.
 */
typedef enum {
  LCI_OK = 0,
  LCI_ERR_RETRY,
  LCI_ERR_FATAL,
} LCI_error_t;

/**
 * LCI Communication type.
 */
typedef enum {
  LCI_COMM_1SIDED,
  LCI_COMM_2SIDED,
  LCI_COMM_COLLECTIVE,
} LCI_comm_t;

/**
 * LCI Matching type.
 */
typedef enum { LCI_MATCH_RANKTAG = 0, LCI_MATCH_TAG } LCI_match_t;

/**
 * LCI Message type.
 */
typedef enum {
  LCI_MSG_IMMEDIATE,
  LCI_MSG_BUFFERED,
  LCI_MSG_DIRECT,
} LCI_msg_t;

/**
 * LCI Port type.
 */
typedef enum {
  LCI_PORT_COMMAND = 0,
  LCI_PORT_MESSAGE = 1,
} LCI_port_t;

/**
 * LCI completion type.
 */
typedef enum {
  LCI_COMPLETION_QUEUE = 0,
  LCI_COMPLETION_HANDLER,
  LCI_COMPLETION_ONE2ONEL,
  LCI_COMPLETION_MANY2ONES,
  LCI_COMPLETION_MANY2ONEL,
  LCI_COMPLETION_ANY2ONES,
  LCI_COMPLETION_ANY2ONEL,
  LCI_COMPLETION_ONE2MANYS,
  LCI_COMPLETION_ONE2MANYL,
  LCI_COMPLETION_MANY2MANYS,
  LCI_COMPLETION_MANY2MANYL
} LCI_comp_t;

/**
 * LCI dynamic buffer.
 */
typedef enum { LCI_STATIC = 0, LCI_DYNAMIC } LCI_dynamic_t;

/**
 * Synchronizer object, owned by the runtime.
 */
typedef uint64_t LCI_sync_t;

typedef char LCI_idata_t[64];
typedef void* LCI_bdata_t;
typedef void* LCI_ddata_t;

typedef union {
  LCI_idata_t immediate;
  LCI_bdata_t buffer;
  LCI_ddata_t direct;
} LCI_data_t;

/**
 * Request object, owned by the user, unless returned from runtime (CQ_Dequeue).
 */
typedef struct {
  /* Status of the communication. */
  LCI_error_t status;
  uint32_t rank;
  uint16_t tag;
  enum { INVALID, IMMEDIATE, BUFFERED, DIRECT } type;
  LCI_data_t data;
  size_t length;
  void* __reserved__;
} LCI_request_t;

typedef struct {
  LCI_sync_t sync;
  LCI_request_t request;
} LCI_syncl_t;

/**
 * Endpoint object, owned by the runtime.
 */
struct LCI_endpoint_s;
typedef struct LCI_endpoint_s* LCI_endpoint_t;

/**
 * Property object, owned by the runtime.
 */
struct LCI_PL_s;
typedef struct LCI_PL_s* LCI_PL_t;

/**
 * Completion queue object, owned by the runtime.
 */
struct LCI_CQ_s;
typedef struct LCI_CQ_s* LCI_CQ_t;

/**
 * Hash table type, owned by the runtime.
 */
struct LCI_MT_s;
typedef struct LCI_MT_s* LCI_MT_t;

/**
 * Handler type
 */
typedef void (*LCI_Handler)(LCI_sync_t* sync, void* usr_context);

/**
 * Allocator type
 */
typedef void* (*LCI_Allocator)(size_t size, void* usr_context);

/**@}*/

/**
 * \defgroup LCISetup LCI Setup, TearDown and Helpers.
 * @{
 */

/**
 * Initialize LCI.
 */
LCI_API
LCI_error_t LCI_initialize(int* argc, char*** args);

/**
 * Finalize LCI.
 */
LCI_API
LCI_error_t LCI_finalize();

/**
 * Create an endpoint Property @plist.
 */
LCI_API
LCI_error_t LCI_PL_create(LCI_PL_t* plist);

/**
 * Destroy an endpoint Property @plist.
 */
LCI_API
LCI_error_t LCI_PL_free(LCI_PL_t* plist);

/**
 * Set communication style (1sided, 2sided, collective).
 */
LCI_API
LCI_error_t LCI_PL_set_comm_type(LCI_comm_t type, LCI_PL_t* plist);

/**
 * Set matching style (ranktag, tag).
 */
LCI_API
LCI_error_t LCI_PL_set_match_type(LCI_match_t type, LCI_PL_t* plist);

/**
 * Set hash-table memory for matching.
 */
LCI_API
LCI_error_t LCI_PL_set_mt(LCI_MT_t* mt, LCI_PL_t* plist);

/**
 * Set message type (short, medium, long).
 */
LCI_API
LCI_error_t LCI_PL_set_msg_type(LCI_msg_t type, LCI_PL_t* plist);

/**
 * Set completion mechanism.
 */
LCI_API
LCI_error_t LCI_PL_set_completion(LCI_port_t port, LCI_comp_t type,
                                  LCI_PL_t* plist);

/**
 * Set completion mechanism.
 */
LCI_API
LCI_error_t LCI_PL_set_cq(LCI_CQ_t* cq, LCI_PL_t* plist);

/**
 * Set handler for AM protocol.
 */
LCI_API
LCI_error_t LCI_PL_set_handler(LCI_Handler handler, LCI_PL_t* plist);

/**
 * Set allocator for dynamic protocol.
 */
LCI_API
LCI_error_t LCI_PL_set_allocator(LCI_Allocator alloc, LCI_PL_t* plist);

/**
 * Create an endpoint, collective calls for those involved in the endpoints.
 */
LCI_API
LCI_error_t LCI_endpoint_create(int device_id, LCI_PL_t plist,
                                LCI_endpoint_t* ep);

/**
 * Query the rank of the current process.
 */
LCI_API
int LCI_Rank();

/**
 * Query the number of processes.
 */
LCI_API
int LCI_Size();

/**
 * Barrier all processes using out-of-band communications.
 */
LCI_API
void LCI_PM_barrier();

/**@}*/

/**
 * \defgroup LCIFuncs LCI communication APIs.
 * @{
 */

/* Two-sided functions. */

/**
 * Send an immediate message, completed immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_sendi(void* src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep);

/**
 * Send a buffered-copy message, completed immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_sendbc(void* src, size_t size, int rank, int tag,
                       LCI_endpoint_t ep);

/**
 * Send a direct message.
 */
LCI_API
LCI_error_t LCI_sendd(void* src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep, void* sync);

/**
 * Receive a immediate message.
 */
LCI_API
LCI_error_t LCI_recvi(void* src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep, void* sync);

/**
 * Receive a medium message.
 */
LCI_API
LCI_error_t LCI_recvbc(void* src, size_t size, int rank, int tag,
                       LCI_endpoint_t ep, void* sync);

/**
 * Receive a direct message.
 */
LCI_API
LCI_error_t LCI_recvd(void* src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep, void* sync);

/* One-sided functions. */

/**
 * Put short message to a remote address @rma_id available at the remote
 * endpoint, offset @offset. Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_puti(void* src, size_t size, int rank, int rma_id, int offset,
                     LCI_endpoint_t ep);

/**
 * Put medium message to a remote address @rma_id available at the remote
 * endpoint, offset @offset. Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_putm(void* src, size_t size, int rank, int rma_id, int offset,
                     LCI_endpoint_t ep);

/**
 * Put long message to a remote address @rma_id available at the remote
 * endpoint, offset @offset.
 */
LCI_API
LCI_error_t LCI_putl(void* src, size_t size, int rank, int rma_id, int offset,
                     LCI_endpoint_t ep, void* context);

/**
 * Put short message to a remote address, piggy-back data to completed request.
 * Complete immediately or LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_putsd(void* src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep);

/**
 * Put medium message to a remote address, piggy-back data to completed request.
 * Complete immediately or LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_putmd(void* src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep);

/**
 * Put long message to a remote address, required a remote allocation.
 */
LCI_API
LCI_error_t LCI_putld(void* src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep, void* sync_context);

/**@}*/

/**
 * \defgroup LCICompl LCI completion mechanism.
 */
/**@{*/

/* Completion methods */

/**
 * Create a completion queue.
 */
LCI_API
LCI_error_t LCI_CQ_create(uint32_t length, LCI_CQ_t* cq);

/**
 * Destroy a completion queue.
 */
LCI_API
LCI_error_t LCI_CQ_free(LCI_CQ_t* cq);

/**
 * Return first completed request in the queue.
 */
LCI_API
LCI_error_t LCI_CQ_dequeue(LCI_CQ_t* cq, LCI_request_t** req);

/**
 * Return at most @n first completed request in the queue.
 */
LCI_API
LCI_error_t LCI_CQ_mul_dequeue(LCI_CQ_t* cq, LCI_request_t requests[],
                               uint8_t count);

/**
 * Return @n requests to the runtime.
 */
LCI_API
LCI_error_t LCI_request_free(LCI_endpoint_t ep, int n, LCI_request_t** req);

/**
 * Create a matching hash-table.
 */
LCI_error_t LCI_MT_create(uint32_t length, LCI_MT_t* mt);

/**
 * Destroy the matching hash-table.
 */
LCI_error_t LCI_MT_free(LCI_MT_t* mt);

/**
 * Create a Sync object.
 */
LCI_API
LCI_error_t LCI_sync_create(void* sync);

/**
 * Reset on a Sync object.
 */
LCI_API
LCI_error_t LCI_one2one_set_full(void* sync);

/**
 * Wait on a Sync object.
 */
LCI_API
LCI_error_t LCI_one2one_set_empty(void* sync);

/**
 * Test a Sync object, return 1 if finished.
 */
LCI_API
int LCI_one2one_test_empty(void* sync);

/**
 * Wait until become full.
 */
LCI_API
int LCI_one2one_wait_full(void* sync);

/**
 * Wait until become empty.
 */
LCI_API
LCI_error_t LCI_one2one_wait_empty(void* sync);

/**
 * Polling a specific device @device_id for at least @count time.
 */
LCI_API
LCI_error_t LCI_progress(int device_id, int count);

/**@}*/

/**
 * Querying a specific device @device_id for a base address.
 */
// LCI_API
// uintptr_t LCI_get_base_addr(int device_id);

#ifdef __cplusplus
}
#endif

#endif
