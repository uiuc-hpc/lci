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

// "pseudo-segment" indicating the entire address space,
// leading to dynamic (on-the-fly) registration
#define LCI_SEGMENT_ALL 1

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
 * @note There is only 15 bits left for tag.
 *       The 16th bit is used to distinguish between user-issued rmas and rmas of sendd.
 */
extern int LCI_MAX_TAG;

/**
 * The maximum size (in byte) of a buffer that can be used in immediate protocol.
 * @note set to 8 bytes (uint64_t) for current implementation
 * @note must be constant at compile time
 * @todo should be larger (LC_MAX_INLINE)
 */
#define LCI_SHORT_SIZE 8

/**
 * The maximum size (in byte) of a buffer that can be used in buffered protocol.
 */
extern int LCI_MEDIUM_SIZE;

/**
 * The maximum number of entries in an IO vector.
 */
extern int LCI_IOVEC_SIZE;

/**
 * The amount of pre-registered memory for a device dedicated for communciation.
 * @todo remove this variable
 */
extern int LCI_REGISTERED_SEGMENT_SIZE;

/**
 * maximum length of a memory segment that can be registered with a device.
 * @todo how to set this?
 */
extern int LCI_MAX_REGISTERED_SEGMENT_SIZE;

/**
 * maximum number of distinct memory segments that can be registered with a device.
 * @todo set this
 */
extern int LCI_MAX_REGISTERED_SEGMENT_NUMBER;

/**
 * initial number of entries in a default matching table.
 * @note The matching table size is fixed in current implementation
 */
extern int LCI_DEFAULT_TABLE_LENGTH;

/**
 * maximum number of entries in a matching table.
 * @note The matching table size is fixed in current implementation
 */
extern int LCI_MAX_TABLE_LENGTH;

/**
 * initial number of entries in a default completion queue.
 * @note The completion queue size is fixed in current implementation
 */
extern int LCI_DEFAULT_QUEUE_LENGTH;

/**
 * maximum number of entries in a completion queue.
 * @note The completion queue size is fixed in current implementation
 */
extern int LCI_MAX_QUEUE_LENGTH;

/**
 * control the LCI log level
 */
extern int LCI_LOG_LEVEL;

/**
 * specify the packet returning threshold
 *
 * Apply to @sendb and @sendbc
 * if size > LCI_PACKET_RETURN_THRESHOLD:
 *   the packet will be returned to the compute thread's pool.
 * else:
 *   the packet will be returned to the progress thread's pool.
 */
extern int LCI_PACKET_RETURN_THRESHOLD;
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
  LCI_ERR_RETRY,      /* Resource temporarily not available. Try again. */
  LCI_ERR_FEATURE_NA, /* Feature not available */
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
  LCI_MSG_SHORT,
  LCI_MSG_MEDIUM,
  LCI_MSG_LONG,
  LCI_MSG_RTS,
  LCI_MSG_RTR,
  LCI_MSG_RDMA
} LCI_msg_type_t;

/**
 * LCI data type.
 */
typedef enum {
  LCI_IMMEDIATE = 0,
  LCI_MEDIUM,
  LCI_LONG,
  LCI_IOVEC
} LCI_data_type_t;

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
 * Tag type. 16 bits due to ibverbs' limitation.
 */
typedef uint16_t LCI_tag_t;

/**
 * LCI generic completion type.
 */
typedef void* LCI_comp_t;

/**
 * Synchronizer object, owned by the runtime.
 */
typedef uint64_t LCI_sync_t;

/**
 * LCI memory segment.
 *
 * All LCI communication must take place in memory segments, which represent
 * memory regions registered to devices.
 */
struct LCI_segment_s;
typedef struct LCI_segment_s *LCI_segment_t;

/**
 * LCI long communication buffer
 */
struct LCI_lbuffer_t {
  LCI_segment_t segment;
  void *address;
  size_t length;
};
typedef struct LCI_lbuffer_t LCI_lbuffer_t;

/**
 * LCI medium communication buffer.
 *
 * Medium communication buffers reside in memory managed by LCI.
 */
struct LCI_mbuffer_t {
  void *address;
  size_t length;
};
typedef struct LCI_mbuffer_t LCI_mbuffer_t;

/**
 * LCI short data.
 */
//struct LCI_short_t {
//  char __short [ LCI_SHORT_SIZE ];
//};
typedef uint64_t LCI_short_t;

/**
 * The type of data associated with a buffer.
 * @todo should we add a flag to identify whether this buffer is allocated by users or LCI?
*/
typedef union {
  LCI_short_t immediate;
  LCI_mbuffer_t mbuffer;
  LCI_lbuffer_t lbuffer;
} LCI_data_t;

/**
 * Request object, owned by the user, unless returned from runtime (CQ_Dequeue).
 * @todo fix the __reserved__ entry
 */
typedef struct {
  /* Status of the communication. */
  LCI_error_t flag;
  uint32_t rank;
  LCI_tag_t tag;
  LCI_data_type_t type;
  LCI_data_t data;
  void* user_context;
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
struct LCI_plist_s;
typedef struct LCI_plist_s* LCI_plist_t;

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
LCI_error_t LCI_open();

/**
 * Finalize LCI.
 */
LCI_API
LCI_error_t LCI_close();

/**
 * Register a memory segment to a device.
 * @param[in]  device  the device to register to
 * @param[in]  address the starting address of the registered memory segment
 * @param[in]  length  the size in bytes of the registered memory segment
 * @param[out] segment a descriptor to the segment
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of failure.
 */
LCI_API
LCI_error_t LCI_memory_register(uint8_t device, void *address, size_t length,
                                LCI_segment_t *segment);

/**
 * Deregister a memory region from a device.
 * @param[in] device  the device to deregister from
 * @param[io] segment a descriptor to the segment to be deregistered, it
 *                will be set to NULL.
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of failure.
 */
LCI_API
LCI_error_t LCI_memory_deregister(uint8_t device, LCI_segment_t *segment);

/**
 * Allocate a buffer from a memory segment
 * @param[in]  size    the desired size of the allocated memory
 * @param[in]  segment the segment where the allocation comes from
 * @param[out] address the starting address of the allocated buffer
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of failure.
 */
LCI_API
LCI_error_t LCI_malloc(size_t size, LCI_segment_t segment, void **address);

/**
 * Free a allocated buffer
 * @param[in] segment the memory segment the buffer belonging to
 * @param[io] address the starting address of the allocated buffer, it will be
 *            set to NULL.
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of failure.
 */
LCI_API
LCI_error_t LCI_free(LCI_segment_t segment, void *address);

/**
 * Create an endpoint Property @plist.
 */
LCI_API
LCI_error_t LCI_plist_create(LCI_plist_t *plist);

/**
 * Destroy an endpoint Property @plist.
 */
LCI_API
LCI_error_t LCI_plist_free(LCI_plist_t *plist);

/**
 * Gets property list attached to an endpoint.
 */
LCI_API
LCI_error_t LCI_plist_get(LCI_endpoint_t endpoint, LCI_plist_t *plist);

/**
 * Returns a string that decodes the property list plist in a human-readable
 * from into string.
 */
LCI_API
LCI_error_t LCI_plist_decode(LCI_plist_t plist, char *string);

/**
 * Set communication style (1sided, 2sided, collective).
 */
LCI_API
LCI_error_t LCI_plist_set_comm_type(LCI_plist_t plist, LCI_comm_t type);

/**
 * Set matching style (ranktag, tag).
 */
LCI_API
LCI_error_t LCI_plist_set_match_type(LCI_plist_t plist, LCI_match_t match_type);

/**
 * Set hash-table memory for matching.
 */
LCI_API
LCI_error_t LCI_plist_set_MT(LCI_plist_t plist, LCI_MT_t* mt);

/**
 * Set message type (short, medium, long).
 */
LCI_API
LCI_error_t LCI_plist_set_msg_type(LCI_plist_t plist, LCI_msg_type_t type);

/**
 * Set completion mechanism.
 */
LCI_API
LCI_error_t LCI_plist_set_completion(LCI_plist_t plist, LCI_port_t port,
                                  LCI_comptype_t type);

/**
 * Set completion mechanism.
 */
LCI_API
LCI_error_t LCI_plist_set_CQ(LCI_plist_t plist, LCI_comp_t* cq);

/**
 * Set handler for AM protocol.
 */
LCI_API
LCI_error_t LCI_plist_set_handler(LCI_plist_t plist, LCI_handler_t* handler);

/**
 * Set dynamic type.
 */
LCI_error_t LCI_plist_set_dynamic(LCI_plist_t	plist, LCI_port_t port,
                               LCI_dynamic_t type);

/**
 * Set allocator for dynamic protocol.
 */
LCI_API
LCI_error_t LCI_plist_set_allocator(LCI_plist_t plist, LCI_allocator_t allocator);

/**
 * Create an endpoint, collective calls for those involved in the endpoints.
 */
LCI_API
LCI_error_t LCI_endpoint_create(int device_id, LCI_plist_t plist,
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
LCI_error_t LCI_sends(LCI_endpoint_t ep, LCI_short_t src, int rank, int tag);

/**
 * Send a buffered-copy message, completed immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_sendm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      int tag);

/**
 * Send a buffered message, not complete until a sync/CQ is done.
 */
LCI_API
LCI_error_t LCI_sendmn(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                       int tag);

/**
 * Send a direct message.
 * @todo API mismatch: LCI_error_t LCI_sendl(LCI_endpoint_t	endpoint,
                                             void 		*buffer,
                                             size_t		length,
                                             uint32_t		destination,
                                             uint16_t		tag,
                                             LCI_comp_t		completion);
         Note: nearly all the communication API has parameter order mismatch.
 */
LCI_API
LCI_error_t LCI_sendl(LCI_endpoint_t ep, LCI_lbuffer_t buffer, uint32_t rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context);

/**
 * Receive a immediate message.
 */
LCI_API
LCI_error_t LCI_recvs(LCI_endpoint_t ep, int rank, int tag,
                      LCI_comp_t completion, void* user_context);

/**
 * Receive a medium message.
 */
LCI_API
LCI_error_t LCI_recvm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      int tag, LCI_comp_t completion, void* user_context);

/**
 * Receive a direct message.
 */
LCI_API
LCI_error_t LCI_recvd(LCI_lbuffer_t src, size_t size, int rank, int tag,
                      LCI_endpoint_t ep, void* sync);

/* One-sided functions. */

/**
 * Put short message to a remote address @p rma_id available at the remote
 * endpoint, offset @p offset. Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_puti(LCI_short_t src, int rank, int rma_id, int offset, int meta,
                     LCI_endpoint_t ep);

/**
 * Put medium message to a remote address @p rma_id available at the remote
 * endpoint, offset @p offset. Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_putbc(LCI_mbuffer_t src, size_t size, int rank, int rma_id, int offset, uint16_t meta,
                     LCI_endpoint_t ep);

/**
 * Put medium message to a remote address @p rma_id available at the remote
 * endpoint, offset @p offset. User must wait for sync or retry if LCI_ERR_RETRY is returned.
 */
LCI_API
LCI_error_t LCI_putb(LCI_mbuffer_t buffer, size_t size, int rank, uint16_t tag,
                     LCI_endpoint_t ep, void* sync);

/**
 * Put long message to a remote address @p rma_id available at the remote
 * endpoint, offset @p offset.
 */
LCI_API
LCI_error_t LCI_putd(LCI_lbuffer_t src, size_t size, int rank, int rma_id, int offset,
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
LCI_error_t LCI_bbuffer_get(LCI_mbuffer_t* buffer, int device_id);

/**
 * @todo API mismatch: LCI_error_t LCI_buffer_free ( LCI_bdata_t buffer );
 */
LCI_API
LCI_error_t LCI_bbuffer_free(LCI_mbuffer_t buffer, int device_id);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif
