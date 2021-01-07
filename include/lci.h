/**
 * @file lci.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 * @brief Header file for all LCI code.
 */

#ifndef LCI_H_
#define LCI_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCI_API __attribute__((visibility("default")))

/**
 * \defgroup LCIGlobal LCI Global variables
 * @{
 */

/**
 * The number of devices created intially.
 */
extern int LCI_NUM_DEVICES;

/**
 * The number of processes in this job.
 */
extern int LCI_NUM_PROCESSES;

/**
 * The rank of the current process w.r.t the job.
 */
extern int LCI_RANK;

/**
 * The maximum number of endpoints that can be created.
 */
extern int LCI_MAX_ENDPOINTS;

/**
 * The largest allowed tag value.
 * @todo integrate it into code logic
 * @note There is only 15 bits left for tag.
 *       The 16th bit is used to distinguish between user-issued rmas and rmas of sendd.
 */
extern int LCI_MAX_TAG;

/**
 * The maximum size (in byte) of a buffer that can be used in immediate protocol.
 * @note set to 8 bytes (uint64_t) for current implementation
 * @todo should be larger (LC_MAX_INLINE)
 */
extern int LCI_IMMEDIATE_SIZE;

/**
 * The maximum size (in byte) of a buffer that can be used in buffered protocol.
 */
extern int LCI_BUFFERED_SIZE;

/**
 * The amount of pre-registered memory for a device dedicated for communciation.
 * @todo unimplemented logic: If the value is zero, then all memory is registered
 *       or registration is not needed.
 */
extern int LCI_REGISTERED_SEGMENT_SIZE;

/**
 * initial address of pre-registered segment. Valid only if
   \ref LCI_REGISTERED_SEGMENT_SIZE is non-zero.
 * @todo unimplemented: what if there are multiple devices?
 * @note invalid due to uncompleted specification
 */
extern int LCI_REGISTERED_SEGMENT_START;

/**
 * maximum length of a memory segment that can be registered with a device.
 */
extern int LCI_MAX_REGISTERED_SEGMENT_SIZE;

/**
 * maximum number of distinct memory segments that can be registered with a device.
 */
extern int LCI_MAX_REGISTERED_SEGMENT_NUMBER;

/**
 * initial number of entries in a default matching table.
 * @note The matching table size is fixed in current implementation
 */
extern int LCI_DEFAULT_MT_LENGTH;

/**
 * maximum number of entries in a matching table.
 * @note The matching table size is fixed in current implementation
 */
extern int LCI_MAX_MT_LENGTH;

/**
 * initial number of entries in a default completion queue.
 * @note The completion queue size is fixed in current implementation
 */
extern int LCI_DEFAULT_CQ_LENGTH;

/**
 * maximum number of entries in a completion queue.
 * @note The completion queue size is fixed in current implementation
 */
extern int LCI_MAX_CQ_LENGTH;

/**
 * control the LCI log level
 */
extern int LCI_LOG_LEVEL;

/**
 * specify the PMI version used by LCI
 */
extern int LCI_PMI_VERSION;
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
typedef enum {
  LCI_MATCH_RANKTAG = 0,
  LCI_MATCH_TAG,
} LCI_match_t;

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
 * LCI completion enumeration type.
 */
typedef enum {
  LCI_COMPLETION_QUEUE = 0,  	// completion queue
  LCI_COMPLETION_HANDLER,  	// handler
  LCI_COMPLETION_ONE2ONES, 	// one2one SSO
  LCI_COMPLETION_ONE2ONEL, 	// one2one LSO
  LCI_COMPLETION_MANY2ONES,	// many2one SSO
  LCI_COMPLETION_MANY2ONEL,	// many2one LSO
  LCI_COMPLETION_ANY2ONES,	// any2one SSO
  LCI_COMPLETION_ONE2MANYS,	// one2many SSO
  LCI_COMPLETION_ONE2MANYL,	// one2many LSO
  LCI_COMPLETION_MANY2MANYS,	// many2many SSO
  LCI_COMPLETION_MANY2MANYL	// many2many LSO
} LCI_comptype_t;

/**
 * LCI dynamic buffer.
 */
typedef enum { LCI_STATIC = 0, LCI_DYNAMIC } LCI_dynamic_t;

/**
 * LCI log level type.
 */
enum LCI_log_level_t {
  LCI_LOG_NONE = 0,
  LCI_LOG_WARN,
  LCI_LOG_TRACE,
  LCI_LOG_INFO,
  LCI_LOG_DEBUG,
  LCI_LOG_MAX
};

/**
 * LCI generic completion type.
 */
typedef void* LCI_comp_t;

/**
 * Synchronizer object, owned by the runtime.
 */
typedef uint64_t LCI_sync_t;

typedef uint64_t LCI_ivalue_t;
typedef void* LCI_bbuffer_t;
typedef void* LCI_dbuffer_t;

/**
 * The type of data associated with a buffer.
 * @todo should we add a flag to identify whether this buffer is allocated by users or LCI?
*/
typedef union {
  LCI_ivalue_t immediate;
  struct {
    void* start;
    size_t length;
  } buffer;
} LCI_buffer_t;

/**
 * Request object, owned by the user, unless returned from runtime (CQ_Dequeue).
 * @todo fix the __reserved__ entry
 */
typedef struct {
  /* Status of the communication. */
  LCI_error_t flag;
  uint32_t rank;
  uint16_t tag;
  enum { INVALID, IMMEDIATE, BUFFERED, DIRECT } type;
  LCI_buffer_t data;
  void* __reserved__;
} LCI_request_t;

/**
 * Synchronizer, owned by the user.
 */
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
// typedef struct LCI_CQ_s* LCI_CQ_t; // replaced by the generic LCI_comp_t

/**
 * Hash table type, owned by the runtime.
 */
struct LCI_MT_s;
typedef struct LCI_MT_s* LCI_MT_t;

/**
 * Handler type
 */
typedef LCI_error_t (LCI_handler_t)(LCI_request_t request);

/**
 * Allocator type
 */
typedef struct {
  void* (*malloc)(size_t size, uint16_t tag);
  void (*free)(void *pointer);
} LCI_allocator_t;

/**@}*/

/**
 * \defgroup LCISetup LCI Setup, TearDown and Helpers.
 * @{
 */

/**
 * Initialize LCI.
 */
LCI_API
LCI_error_t LCI_Init(int* argc, char*** args);

/**
 * Finalize LCI.
 */
LCI_API
LCI_error_t LCI_Free();

/**
 * Create an endpoint Property @plist.
 */
LCI_API
LCI_error_t LCI_PL_create(LCI_PL_t* plist);

/**
 * Destroy an endpoint Property @plist.
 */
LCI_API
LCI_error_t LCI_PL_free(LCI_PL_t plist);

/**
 * Gets property list attached to an endpoint.
 * @todo Not implemented. A large part of PL logic is not implemented.
 *       The property list is returned as a string representation in XML format?
 */
LCI_API
LCI_error_t LCI_PL_get(LCI_endpoint_t endpoint, LCI_PL_t plist);

/**
 * Set communication style (1sided, 2sided, collective).
 */
LCI_API
LCI_error_t LCI_PL_set_comm_type(LCI_PL_t plist, LCI_comm_t type);

/**
 * Set matching style (ranktag, tag).
 */
LCI_API
LCI_error_t LCI_PL_set_match_type(LCI_PL_t plist, LCI_match_t match_type);

/**
 * Set hash-table memory for matching.
 */
LCI_API
LCI_error_t LCI_PL_set_MT(LCI_PL_t plist, LCI_MT_t* mt);

/**
 * Set message type (short, medium, long).
 */
LCI_API
LCI_error_t LCI_PL_set_msg_type(LCI_PL_t plist, LCI_msg_t type);

/**
 * Set completion mechanism.
 */
LCI_API
LCI_error_t LCI_PL_set_completion(LCI_PL_t plist, LCI_port_t port,
                                  LCI_comptype_t type);

/**
 * Set completion mechanism.
 */
LCI_API
LCI_error_t LCI_PL_set_CQ(LCI_PL_t plist, LCI_comp_t* cq);

/**
 * Set handler for AM protocol.
 */
LCI_API
LCI_error_t LCI_PL_set_handler(LCI_PL_t plist, LCI_handler_t* handler);

/**
 * Set dynamic type.
 */
LCI_error_t LCI_PL_set_dynamic(LCI_PL_t	plist, LCI_port_t port,
                               LCI_dynamic_t type);

/**
 * Set allocator for dynamic protocol.
 */
LCI_API
LCI_error_t LCI_PL_set_allocator(LCI_PL_t plist, LCI_allocator_t allocator);

/**
 * Create an endpoint, collective calls for those involved in the endpoints.
 */
LCI_API
LCI_error_t LCI_endpoint_create(int device_id, LCI_PL_t plist,
                                LCI_endpoint_t* ep);

/**
 * Free an endpoint, collective calls for those involved in the endpoints.
 * @todo not implemented: endpoints are stored in an array.
 */
LCI_API
LCI_error_t LCI_endpoint_free(LCI_endpoint_t *endpoint);

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
void LCI_barrier();

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
LCI_error_t LCI_sendi(LCI_ivalue_t value, int rank, int tag,
                      LCI_endpoint_t ep);

/**
 * Send a buffered-copy message, completed immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_sendbc(void* src, size_t size, int rank, int tag,
                       LCI_endpoint_t ep);

/**
 * Send a buffered message, not complete until a sync/CQ is done.
 */
LCI_API
LCI_error_t LCI_sendb(LCI_bbuffer_t src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep, void* sync);

/**
 * Send a direct message.
 * @todo API mismatch: LCI_error_t LCI_sendd(LCI_endpoint_t	endpoint,
                                             void 		*buffer,
                                             size_t		length,
                                             uint32_t		destination,
                                             uint16_t		tag,
                                             LCI_comp_t		completion);
         Note: nearly all the communication API has parameter order mismatch.
 */
LCI_API
LCI_error_t LCI_sendd(LCI_dbuffer_t src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep, void* sync);

/**
 * Receive a immediate message.
 */
LCI_API
LCI_error_t LCI_recvi(LCI_ivalue_t* src, int rank, int tag,
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
LCI_error_t LCI_recvd(LCI_dbuffer_t src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep, void* sync);

/* One-sided functions. */

/**
 * Put short message to a remote address @p rma_id available at the remote
 * endpoint, offset @p offset. Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_puti(LCI_ivalue_t src, int rank, int rma_id, int offset, int meta,
                     LCI_endpoint_t ep);

/**
 * Put medium message to a remote address @p rma_id available at the remote
 * endpoint, offset @p offset. Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_putbc(LCI_bbuffer_t src, size_t size, int rank, int rma_id, int offset, uint16_t meta,
                     LCI_endpoint_t ep);

/**
 * Put medium message to a remote address @p rma_id available at the remote
 * endpoint, offset @p offset. User must wait for sync or retry if LCI_ERR_RETRY is returned.
 */
LCI_API
LCI_error_t LCI_putb(LCI_bbuffer_t buffer, size_t size, int rank, uint16_t tag,
                     LCI_endpoint_t ep, void* sync);

/**
 * Put long message to a remote address @p rma_id available at the remote
 * endpoint, offset @p offset.
 */
LCI_API
LCI_error_t LCI_putd(LCI_dbuffer_t src, size_t size, int rank, int rma_id, int offset,
                     LCI_endpoint_t ep, void* context);

/**@}*/

/**
 * \defgroup LCICompl LCI completion mechanism.
 */
/**@{*/

/* Completion methods */

/**
 * Create a completion queue.
 * @todo Current completion queue implementation has the @p length hardcoded as CQ_MAX_SIZE.
 *       The memory is also allocated as a static array. Do we want to change the implementation?
 *       Or do we want to change the manual? Need to check with others.
 */
LCI_API
LCI_error_t LCI_CQ_init(LCI_comp_t* cq, uint32_t length);

/**
 * Destroy a completion queue.
 */
LCI_API
LCI_error_t LCI_CQ_free(LCI_comp_t cq);

/**
 * Return first completed request in the queue. Unblocking.
 * @todo API mismatch: LCI_error_t LCI_dequeue ( LCI_comp_t cq , LCI_request_t * request );
 *       Do we want to return the LCI_request_t by pointers (implementation) or actual objects (manual)
 */
LCI_API
LCI_error_t LCI_dequeue(LCI_comp_t cq, LCI_request_t** request);

/**
 * Return first completed request in the queue. Blocking.
 * @todo API mismatch: Do we want to return the LCI_request_t by pointers (implementation) or actual objects (manual)
 */
LCI_API
LCI_error_t LCI_wait_dequeue(LCI_comp_t cq, LCI_request_t** request);

/**
 * Return at most @p count first completed request in the queue.
 */
LCI_API
LCI_error_t LCI_mult_dequeue(LCI_comp_t cq ,
                             LCI_request_t requests[] ,
                             uint32_t request_count ,
                             uint32_t *return_count );

/**
 * Create a matching hash-table.
 * @todo Current hashtable implementation hardcodes the @p length to be (1 << TBL_BIT_SIZE)
 *       One possible fix would be hash = hash % lc_hash_size.
 *       Do we want to change the implementation or the manual?
 *       need to check with others
 */
LCI_API
LCI_error_t LCI_MT_init(LCI_MT_t* mt, uint32_t length);

/**
 * Destroy the matching hash-table.
 */
LCI_API
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
LCI_error_t LCI_one2one_wait_full(void* sync);

/**
 * Wait until become empty.
 */
LCI_API
LCI_error_t LCI_one2one_wait_empty(void* sync);

/**
 * Polling a specific device @p device_id for at least @p count time.
 */
LCI_API
LCI_error_t LCI_progress(int device_id, int count);

/**
 * Querying a specific device @device_id for a base address.
 */
LCI_API
uintptr_t LCI_get_base_addr(int device_id);

/**
 * @todo API mismatch: LCI_error_t LCI_buffer_get(LCI_bdata_t* buffer, uint8 device_id);
 */
LCI_API
LCI_error_t LCI_bbuffer_get(LCI_bbuffer_t* buffer, int device_id);

/**
 * @todo API mismatch: LCI_error_t LCI_buffer_free ( LCI_bdata_t buffer );
 */
LCI_API
LCI_error_t LCI_bbuffer_free(LCI_bbuffer_t buffer, int device_id);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif
