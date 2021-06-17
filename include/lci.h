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

#define LCI_UR_CQ_REMOTE 0

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
 *
 * @note used by LCII_MAKE_PROTO (3 bits) for communication immediate data field
 * and LCII_MAKE_KEY (2 bits, only use the first three entries) for the matching
 * table key. Take care when modify this enum type.
 */
typedef enum {
  LCI_MSG_SHORT,
  LCI_MSG_MEDIUM,
  LCI_MSG_LONG,
  LCI_MSG_RTS,
  LCI_MSG_RTR,
  LCI_MSG_RDMA_SHORT,
  LCI_MSG_RDMA_MEDIUM,
  LCI_MSG_RDMA_LONG,
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
  LCI_COMPLETION_SYNC, 	        // synchronizer
  LCI_COMPLETION_FREE, 	        // just free the packet, no completion reported to users
  LCI_COMPLETION_NONE
} LCI_comp_type_t;

/**
 * LCI synchronizer type
 */
typedef enum {
  LCI_SYNC_SIMPLE = 0,
} LCI_sync_type_t;

/**
 * Tag type. 16 bits due to ibverbs' limitation.
 */
typedef uint16_t LCI_tag_t;

/**
 * LCI generic completion type.
 */
typedef void* LCI_comp_t;

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
  LCI_short_t immediate;  // 8 bytes
  LCI_mbuffer_t mbuffer;  // 16 bytes
  LCI_lbuffer_t lbuffer;  // 24 bytes
} LCI_data_t;

/**
 * Request object, owned by the user, unless returned from runtime (CQ_Dequeue).
 * @todo fix the __reserved__ entry
 */
typedef struct {
  /* Status of the communication. */
  LCI_error_t flag;       // 4 bytes
  uint32_t rank;          // 4 bytes
  LCI_tag_t tag;          // 4 bytes
  LCI_data_type_t type;   // 4 bytes
  LCI_data_t data;        // 24 bytes
  void* user_context;     // 8 bytes
} LCI_request_t;

/**
 * The device object
 */
struct LCI_device_s;
typedef struct LCI_device_s* LCI_device_t;

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
 * \defgroup LCIGlobal LCI Global variables
 * @{
 */

// "pseudo-segment" indicating the entire address space,
// leading to dynamic (on-the-fly) registration
#define LCI_SEGMENT_ALL 1

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
 * specify the packet returning threshold
 *
 * Apply to @sendb and @sendbc
 * if size > LCI_PACKET_RETURN_THRESHOLD:
 *   the packet will be returned to the compute thread's pool.
 * else:
 *   the packet will be returned to the progress thread's pool.
 */
extern int LCI_PACKET_RETURN_THRESHOLD;

/**
 * @brief The endpoint created during LCI initialization
 *
 * LCI initialization creates on each process an endpoint LCI_UR_ENDPOINT on
 * device 0 which is associated with the default completion queue LCI_QUEUE[0]
 * and with memory segment LCI_MEMORY_ALL and supports all message types
 * and all types of communications.
 */
extern LCI_device_t LCI_UR_DEVICE;
extern LCI_endpoint_t LCI_UR_ENDPOINT;
extern LCI_comp_t LCI_UR_CQ;
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
LCI_error_t LCI_plist_get(LCI_endpoint_t ep, LCI_plist_t *plist_ptr);

/**
 * Returns a string that decodes the property list plist in a human-readable
 * from into string.
 */
LCI_API
LCI_error_t LCI_plist_decode(LCI_plist_t plist, char *string);

/**
 * Set matching style (ranktag, tag).
 */
LCI_API
LCI_error_t LCI_plist_set_match_type(LCI_plist_t plist, LCI_match_t match_type);

/**
 * Set completion type
 * @param plist the property list to set
 * @param port specify command port or message port to set
 * @param comp_type specify the completion type
 * @return A value of zero indicates success while a nonzero value indicates
 *         failure. Different values may be used to indicate the type of failure.
 */
LCI_API
LCI_error_t LCI_plist_set_comp_type(LCI_plist_t plist, LCI_port_t port,
                                    LCI_comp_type_t comp_type);
/**
 * Set allocator for dynamic protocol.
 */
LCI_API
LCI_error_t LCI_plist_set_allocator(LCI_plist_t plist,
                                    LCI_allocator_t allocator);

/**
 * Create an endpoint, collective calls for those involved in the endpoints.
 */
LCI_API
LCI_error_t LCI_endpoint_init(LCI_endpoint_t* ep_ptr, LCI_device_t device,
                              LCI_plist_t plist);

/**
 * Free an endpoint, collective calls for those involved in the endpoints.
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
LCI_error_t LCI_sends(LCI_endpoint_t ep, LCI_short_t src, int rank, LCI_tag_t tag);

/**
 * Send a buffered-copy message, completed immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_sendm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag);

/**
 * Send a buffered message, not complete until a sync/CQ is done.
 */
LCI_API
LCI_error_t LCI_sendmn(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                       LCI_tag_t tag);

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
LCI_error_t LCI_recvs(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                      LCI_comp_t completion, void* user_context);

/**
 * Receive a medium message.
 */
LCI_API
LCI_error_t LCI_recvm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context);

/**
 * Receive a medium message with LCI-provided buffer.
 */
LCI_API
LCI_error_t LCI_recvmn(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                       LCI_comp_t completion, void* user_context);

/**
 * Receive a direct message.
 */
LCI_API
LCI_error_t LCI_recvl(LCI_endpoint_t ep, LCI_lbuffer_t buffer, uint32_t rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context);

/* One-sided functions. */
LCI_API
LCI_error_t LCI_puts(LCI_endpoint_t ep, LCI_short_t src, int rank,
                     LCI_tag_t tag, uintptr_t remote_completion);
LCI_API
LCI_error_t LCI_putma(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag, uintptr_t remote_completion);
LCI_API
LCI_error_t LCI_putmna(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag, uintptr_t remote_completion);
LCI_API
LCI_error_t LCI_putla(LCI_endpoint_t ep, LCI_lbuffer_t buffer,
                      LCI_comp_t completion, int rank, LCI_tag_t tag,
                      uintptr_t remote_completion, void* user_context);

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
LCI_error_t LCI_queue_create(LCI_device_t device, LCI_comp_t* cq);

/**
 * Destroy a completion queue.
 */
LCI_API
LCI_error_t LCI_queue_free(LCI_comp_t* cq);

/**
 * Return first completed request in the queue. Unblocking.
 * @todo API mismatch: LCI_error_t LCI_queue_pop ( LCI_comp_t cq , LCI_request_t * request );
 *       Do we want to return the LCI_request_t by pointers (implementation) or actual objects (manual)
 */
LCI_API
LCI_error_t LCI_queue_pop(LCI_comp_t cq, LCI_request_t* request);

/**
 * Return first completed request in the queue. Blocking.
 * @todo API mismatch: Do we want to return the LCI_request_t by pointers (implementation) or actual objects (manual)
 */
LCI_API
LCI_error_t LCI_queue_wait(LCI_comp_t cq, LCI_request_t* request);

/**
 * Return at most @p count first completed request in the queue.
 */
LCI_API
LCI_error_t LCI_queue_pop_multiple(LCI_comp_t cq, uint32_t request_count,
                                   LCI_request_t* requests,
                                   uint32_t* return_count);

/**
 * Blocking wait and return @p count first completed request in the queue.
 */
LCI_API
LCI_error_t LCI_queue_wait_multiple(LCI_comp_t cq, uint32_t request_count,
                                   LCI_request_t* requests);

/**
 * Create a Sync object.
 */
LCI_API
LCI_error_t LCI_sync_create(LCI_device_t device, LCI_sync_type_t sync_type,
                            LCI_comp_t* sync);
LCI_API
LCI_error_t LCI_sync_free(LCI_comp_t *completion);

/**
 * Reset on a Sync object.
 */
LCI_API
LCI_error_t LCI_sync_wait(LCI_comp_t sync, LCI_request_t* request);
LCI_API
LCI_error_t LCI_sync_test(LCI_comp_t completion, LCI_request_t* request);

/**
 * Polling a specific device @p device_id for at least @p count time.
 */
LCI_API
LCI_error_t LCI_progress(LCI_device_t device);

// memory management
LCI_API
LCI_error_t LCI_memory_register(LCI_device_t device, void *address, size_t length,
                                LCI_segment_t *segment);
LCI_API
LCI_error_t LCI_memory_deregister(LCI_segment_t* segment);
LCI_API
LCI_error_t LCI_mbuffer_alloc(LCI_device_t device, LCI_mbuffer_t* mbuffer);
LCI_API
LCI_error_t LCI_mbuffer_free(LCI_mbuffer_t mbuffer);
LCI_API
LCI_error_t LCI_lbuffer_alloc(LCI_device_t device, size_t size, LCI_lbuffer_t* lbuffer);
LCI_API
LCI_error_t LCI_lbuffer_memalign(LCI_device_t device, size_t size, size_t alignment, LCI_lbuffer_t* lbuffer);
LCI_API
LCI_error_t LCI_lbuffer_free(LCI_lbuffer_t lbuffer);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif
