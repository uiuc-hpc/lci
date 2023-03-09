/**
 * @file lci.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 *         Omri Mor (omrimor2@illinois.edu)
 *         Jiakun Yan (jiakuny3@illinois.edu)
 * @brief Header file for all LCI code.
 */

#ifndef LCI_H_
#define LCI_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "lci_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup LCI_API Lightweight Communication Interface (LCI) API
 * @{
 * This section describes LCI API.
 * @}
 */

/**
 * @defgroup LCI_COMP LCI completion mechanisms
 * @ingroup LCI_API
 * @{
 * The LCI runtime notifies users about the completion of an asynchronous
 * operations (e.g. communication operations) through completion
 * mechanisms.
 *
 * LCI provides three completion mechanisms: synchronizers,
 * completion queues, and active message handlers.
 *
 * @}
 */

#define LCI_API __attribute__((visibility("default")))

#define LCI_DEFAULT_COMP_REMOTE 0
// "pseudo-segment" indicating the entire address space,
// leading to dynamic (on-the-fly) registration
#define LCI_SEGMENT_ALL NULL
#define LCI_RANK_ANY (-1)

/**
 * \defgroup LCITypes LCI Data Types.
 * @{
 */

/**
 * LCI Error type.
 */
typedef enum {
  LCI_OK = 0,
  LCI_ERR_RETRY,       /* Resource temporarily not available. Try again. */
  LCI_ERR_RETRY_LOCK,  /* Resource temporarily not available due to lock
                          contention. Try again. */
  LCI_ERR_RETRY_NOMEM, /* Resource temporarily not available due to no memory.
                          Try again. */
  LCI_ERR_FEATURE_NA,  /* Feature not available */
  LCI_ERR_FATAL,
} LCI_error_t;

/**
 * LCI Matching type.
 */
typedef enum {
  LCI_MATCH_RANKTAG = 0,
  LCI_MATCH_TAG,
} LCI_match_t;

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
  LCI_COMPLETION_NONE = 0,
  LCI_COMPLETION_QUEUE,    // completion queue
  LCI_COMPLETION_HANDLER,  // handler
  LCI_COMPLETION_SYNC,     // synchronizer
} LCI_comp_type_t;

/**
 * Tag type. the lower 16 bits are actually used due to ibverbs' limitation.
 */
typedef uint32_t LCI_tag_t;

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
struct LCII_mr_t;
typedef struct LCII_mr_t* LCI_segment_t;

/**
 * LCI long communication buffer
 */
struct LCI_lbuffer_t {
  LCI_segment_t segment;
  void* address;
  size_t length;
};
typedef struct LCI_lbuffer_t LCI_lbuffer_t;

/**
 * LCI medium communication buffer.
 *
 * Medium communication buffers reside in memory managed by LCI.
 */
struct LCI_mbuffer_t {
  void* address;
  size_t length;
};
typedef struct LCI_mbuffer_t LCI_mbuffer_t;

/**
 * LCI short data.
 */
#define LCI_SHORT_SIZE 32
typedef union {
#ifdef LCI_USE_AVX
  uint8_t u8 __attribute__((vector_size(32)));
  uint16_t u16 __attribute__((vector_size(32)));
  uint32_t u32 __attribute__((vector_size(32)));
  uint64_t u64 __attribute__((vector_size(32)));
  int8_t s8 __attribute__((vector_size(32)));
  int16_t s16 __attribute__((vector_size(32)));
  int32_t s32 __attribute__((vector_size(32)));
  int64_t s64 __attribute__((vector_size(32)));
  double dbl __attribute__((vector_size(32)));
  float flt __attribute__((vector_size(32)));
#else
  char val[LCI_SHORT_SIZE];
#endif
} LCI_short_t;
// typedef uint64_t LCI_short_t;

/**
 * LCI IOVEC.
 */
struct LCI_iovec_t {
  LCI_mbuffer_t piggy_back;  // 16 bytes
  LCI_lbuffer_t* lbuffers;   // 8 bytes
  int count;                 // 4 bytes
};
typedef struct LCI_iovec_t LCI_iovec_t;  // 28 bytes

/**
 * The type of data associated with a buffer.
 * @todo should we add a flag to identify whether this buffer is allocated by
 * users or LCI?
 */
typedef union {
  LCI_short_t immediate;  // 32 bytes
  LCI_mbuffer_t mbuffer;  // 16 bytes
  LCI_lbuffer_t lbuffer;  // 24 bytes
  LCI_iovec_t iovec;      // 28 bytes
} LCI_data_t;

/**
 * Request object, owned by the user.
 */
typedef struct {
  /* Status of the communication. */
  LCI_error_t flag;      // 4 bytes
  int rank;              // 4 bytes
  LCI_tag_t tag;         // 4 bytes
  LCI_data_type_t type;  // 4 bytes
  LCI_data_t data;       // 32 bytes
  void* user_context;    // 8 bytes
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
typedef void (*LCI_handler_t)(LCI_request_t request);

/**@}*/

/**
 * \defgroup LCIGlobal LCI Global variables
 * @{
 */

/**
 * Basic configuration
 */
extern uint64_t LCI_PAGESIZE;

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
 *       The 16th bit is used to distinguish between user-issued rmas and rmas
 * of sendd.
 */
extern int LCI_MAX_TAG;

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
 */
extern int LCI_REGISTERED_SEGMENT_SIZE;

/**
 * maximum length of a memory segment that can be registered with a device.
 */
extern int LCI_MAX_REGISTERED_SEGMENT_SIZE;

/**
 * maximum number of distinct memory segments that can be registered with a
 * device.
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
 * maximum number of request a synchronizer may be waiting for.
 */
extern int LCI_MAX_SYNC_LENGTH;

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
 * Whether or what to use IBV on-demand paging:
 * 0: Don't use ODP
 * 1: Use explicit ODP
 * 2: Use implicit ODP
 */
extern int LCI_IBV_USE_ODP;

/**
 * Whether touch every page of lbuffers allocated by LCI_lbuffer_alloc
 */
extern int LCI_TOUCH_LBUFFER;

/**
 * Whether or what to use LCI-provided registration cache:
 * 0: Don't use.
 * 1: Use.
 */
extern int LCI_USE_DREG;

/**
 * Whether or what to use IBV prefetch:
 * 0: Don't use.
 * 1: Use. (Only make sense when LCI_IBV_USE_ODP is 1 or 2)
 */
extern int LCI_IBV_USE_PREFETCH;

/**
 * Total number of packets
 */
extern int LCI_PACKET_SIZE;

/**
 * Total number of packets
 */
extern int LCI_SERVER_NUM_PKTS;

/**
 * Server's send queue length
 */
extern int LCI_SERVER_MAX_SENDS;

/**
 * Server's recv queue length
 */
extern int LCI_SERVER_MAX_RECVS;

/**
 * Server's completion queue length
 */
extern int LCI_SERVER_MAX_CQES;

/**
 * whether to enable ibv's event polling thread
 */
extern bool LCI_IBV_ENABLE_EVENT_POLLING_THREAD;

/**
 * LCI_progress will not be called by multiple threads simultaneously
 */
extern int LCI_SINGLE_THREAD_PROGRESS;

/**
 * Manually slow down LCI backend send function
 */
extern int LCI_SEND_SLOW_DOWN_USEC;

/**
 * Manually slow down LCI backend recv function
 */
extern int LCI_RECV_SLOW_DOWN_USEC;

extern LCI_device_t LCI_UR_DEVICE;
extern LCI_endpoint_t LCI_UR_ENDPOINT;
extern LCI_comp_t LCI_UR_CQ;
/**@}*/

/**
 * @defgroup LCI_SETUP LCI environment setup
 * @ingroup LCI_API
 * @{
 * In order to use LCI, users need to first setup a few communication resources
 * and configurations, including devices and endpoints.
 *
 * @}
 */

/**
 * @ingroup LCI_SETUP
 * @brief Initialize the LCI runtime. No LCI calls are allowed to be called
 * before LCI_initialize except @ref LCI_initialized.
 * @return Error code as defined by @ref LCI_error_t
 */
LCI_API
LCI_error_t LCI_initialize();
/**
 * @ingroup LCI_SETUP
 * @brief Check whether the LCI runtime has been initialized
 * @param [in] flag If the runtime has been initialized, it will be set to true.
 * Otherwise, it will be set to false.
 * @return Error code as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_initialized(int* flag);
/**
 * @ingroup LCI_SETUP
 * @brief Finalize the LCI runtime. No LCI calls are allowed to be called
 * after LCI_finalize except @ref LCI_initialized.
 * @return Error code as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_finalize();
/**
 * @ingroup LCI_SETUP
 * @brief Invoke a barrier across all LCI processes in the same job. This is
 * not thread-safe.
 * @return Error code as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_barrier();

/**
 * @defgroup LCI_DEVICE LCI device
 * @ingroup LCI_API
 * @{
 * A device is a physical or logical resource that can be used for
 * communication. Communications that use the same device share a resource and
 * may affect each other's performance.
 * @}
 */
/**
 * @ingroup LCI_SETUP
 * @brief Initialize a device.
 * @param [out] device pointer to a device to be initialized.
 * @return Error code as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_device_init(LCI_device_t* device);
/**
 * @ingroup LCI_SETUP
 * @brief Initialize a device.
 * @param [out] device pointer to a device to be freed.
 * @return Error code as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_device_free(LCI_device_t* device);
// plist
LCI_API
LCI_error_t LCI_plist_create(LCI_plist_t* plist);
LCI_API
LCI_error_t LCI_plist_free(LCI_plist_t* plist);
LCI_API
LCI_error_t LCI_plist_get(LCI_endpoint_t ep, LCI_plist_t* plist_ptr);
LCI_API
LCI_error_t LCI_plist_decode(LCI_plist_t plist, char* string);
LCI_API
LCI_error_t LCI_plist_set_match_type(LCI_plist_t plist, LCI_match_t match_type);
LCI_API
LCI_error_t LCI_plist_set_comp_type(LCI_plist_t plist, LCI_port_t port,
                                    LCI_comp_type_t comp_type);
LCI_API
LCI_error_t LCI_plist_set_default_comp(LCI_plist_t plist, LCI_comp_t comp);
// endpoint
LCI_API
LCI_error_t LCI_endpoint_init(LCI_endpoint_t* ep_ptr, LCI_device_t device,
                              LCI_plist_t plist);
LCI_API
LCI_error_t LCI_endpoint_free(LCI_endpoint_t* endpoint);
// two-sided functions
LCI_API
LCI_error_t LCI_sends(LCI_endpoint_t ep, LCI_short_t src, int rank,
                      LCI_tag_t tag);
LCI_API
LCI_error_t LCI_sendm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag);
LCI_API
LCI_error_t LCI_sendmn(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                       LCI_tag_t tag);
LCI_API
LCI_error_t LCI_sendl(LCI_endpoint_t ep, LCI_lbuffer_t buffer, uint32_t rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context);
LCI_API
LCI_error_t LCI_recvs(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                      LCI_comp_t completion, void* user_context);
LCI_API
LCI_error_t LCI_recvm(LCI_endpoint_t ep, LCI_mbuffer_t buffer, int rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context);
LCI_API
LCI_error_t LCI_recvmn(LCI_endpoint_t ep, int rank, LCI_tag_t tag,
                       LCI_comp_t completion, void* user_context);
LCI_API
LCI_error_t LCI_recvl(LCI_endpoint_t ep, LCI_lbuffer_t buffer, uint32_t rank,
                      LCI_tag_t tag, LCI_comp_t completion, void* user_context);
// one-sided functions
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
LCI_API
LCI_error_t LCI_putva(LCI_endpoint_t ep, LCI_iovec_t iovec,
                      LCI_comp_t completion, int rank, LCI_tag_t tag,
                      uintptr_t remote_completion, void* user_context);

/**
 * @defgroup LCI_SYNC LCI synchronizer
 * @ingroup LCI_COMP
 * @{
 * One of the completion mechanisms.
 *
 * In its simplest form, it is an object similar to MPI_Request.
 * Every synchronizer can be associated with one or more
 * asynchronous operations. The synchronizer will be triggered after all the
 * associated operations are completed. Users can know whether the corresponding
 * operations have been completed by querying the synchronizer.
 *
 * It can also be scheduler-aware when executing in the context of
 * a task-based runtime (This is still an ongoing work).
 * @}
 */

/**
 * @defgroup LCI_QUEUE LCI completion queue
 * @ingroup LCI_COMP
 * @{
 * One of the completion mechanisms.
 *
 * Every completion queue can be associated with one or more
 * asynchronous operations. When a operation is completed,
 * the LCI runtime will push an entry into the queue, users can
 * get completion notification by calling @ref LCI_cq_pop.
 * @}
 */

/**
 * @ingroup LCI_QUEUE
 * @brief Create a completion queue.
 *
 * @param [in]  device The device it should be associated to.
 * @param [out] cq     The pointer to the completion queue to be created.
 * @return Error code as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_queue_create(LCI_device_t device, LCI_comp_t* cq);
/**
 * @ingroup LCI_QUEUE
 * @brief Free a completion queue.
 *
 * @param [in] cq The pointer to the completion queue to be freed.
 * @return Error code as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_queue_free(LCI_comp_t* cq);
/**
 * @ingroup LCI_QUEUE
 * @brief Pop one entry from the completion queue. This call is nonblocking.
 *
 * @param [in]  cq      The completion queue to pop.
 * @param [out] request The pointer to a request object. The completion
 * information of the completed operation will be written into it.
 * @return LCI_OK if successfully popped one. LCI_ERR_RETRY if there is no entry
 * to be popped in the queue. All the other errors are fatal as defined by
 * @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_queue_pop(LCI_comp_t cq, LCI_request_t* request);
/**
 * @ingroup LCI_QUEUE
 * @brief Pop one entry from the completion queue. This call will block until
 * there is an entry to be popped.
 *
 * @param [in]  cq      The completion queue to pop.
 * @param [out] request The pointer to a request object. The completion
 * information of the completed operation will be written into it.
 * @return LCI_OK if successfully popped one. All the other errors are fatal
 * as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_queue_wait(LCI_comp_t cq, LCI_request_t* request);
/**
 * @ingroup LCI_QUEUE
 * @brief Pop multiple entry from the completion queue. This call is
 * nonblocking.
 *
 * @param [in]  cq            The completion queue to pop.
 * @param [in]  request_count The maximum entries to be popped.
 * @param [out] request       An array of request objects of length at least
 * `request_count`.Each of the fist `return_count` entries of the array will
 * contain the completion information of one completed operation.
 * @param [out] request_count The number of entries that has been popped.
 * @return Should always be LCI_OK. All the other errors are fatal
 * as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_queue_pop_multiple(LCI_comp_t cq, size_t request_count,
                                   LCI_request_t* requests,
                                   size_t* return_count);
/**
 * @ingroup LCI_QUEUE
 * @brief Pop multiple entry from the completion queue.This call will block
 * until `request_count` entries has been popped.
 *
 * @param [in]  cq            The completion queue to pop.
 * @param [in]  request_count The number of entries to be popped.
 * @param [out] request       An array of request objects of length at least
 * `request_count`.Each entry of the array will contain the completion
 * information of one completed operation.
 * @return Should always be LCI_OK. All the other errors are fatal
 * as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_queue_wait_multiple(LCI_comp_t cq, size_t request_count,
                                    LCI_request_t* requests);
/**
 * @ingroup LCI_QUEUE
 * @brief Query the number of entries in a completion queue.
 *
 * @param [in]  cq  The completion queue to query.
 * @param [out] len Return the number of entries in the queue.
 * @return No available yet. Should always be LCI_ERR_FEATURE_NA.
 */
LCI_API
LCI_error_t LCI_queue_len(LCI_comp_t cq, size_t* len);

/**
 * @ingroup LCI_SYNC
 * @brief Create a synchronizer.
 *
 * @param [in]  device     The device it should be associated to.
 * @param [in]  threshold  How many asynchronous operations it will be
 * associated to.
 * @param [out] completion The pointer to the synchronizer to be created.
 * @return Should always be LCI_OK. All the other errors are fatal
 * as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_sync_create(LCI_device_t device, int threshold,
                            LCI_comp_t* completion);
/**
 * @ingroup LCI_SYNC
 * @brief Free a synchronizer.
 *
 * @param [in] completion The pointer to the synchronizer to be freed.
 * @return Should always be LCI_OK. All the other errors are fatal
 * as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_sync_free(LCI_comp_t* completion);
/**
 * @ingroup LCI_SYNC
 * @brief Trigger a synchronizer.
 *
 * @param [in]  completion The synchronizer to be triggered.
 * @param [in] request     An array of size `threshold` passed to
 * @ref LCI_sync_create. Every entry of the array will contain the completion
 * information of one completed operation.
 * @return Should always be LCI_OK. All the other errors are fatal
 * as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_sync_signal(LCI_comp_t completion, LCI_request_t request);
/**
 * @ingroup LCI_SYNC
 * @brief Wait for a synchronizer to be triggered. A synchronizer will
 * be triggered if all the associated operations are completed.
 *
 * @param [in]  completion The synchronizer to be waited.
 * @param [out] request    An array of length `threshold` passed to
 * @ref LCI_sync_create. Every entry of the array will contain the completion
 * information of one completed operation.
 * @return Should always be LCI_OK. All the other errors are fatal
 * as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_sync_wait(LCI_comp_t completion, LCI_request_t request[]);
/**
 * @ingroup LCI_SYNC
 * @brief Test whether a synchronizer has been triggered. A synchronizer will
 * be triggered if all the associated operations are completed.
 *
 * @param [in]  completion The synchronizer to be tested.
 * @param [out] request    An array of length `threshold` passed to
 * @ref LCI_sync_create. Every entry of the array will contain the completion
 * information of one completed operation.
 * @return LCI_OK if successfully popped one. LCI_ERR_RETRY if there is no entry
 * to be popped in the queue. All the other errors are fatal as defined by
 * @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_sync_test(LCI_comp_t completion, LCI_request_t request[]);

/**
 * @defgroup LCI_HANDLER LCI active message handler
 * @ingroup LCI_COMP
 * @{
 * One of the completion mechanisms.
 *
 * Every active message handler can be associated with one or more
 * asynchronous operations. When operations are completed,
 * the LCI runtime will execute the handler with the completion entry
 * as the input.
 * @}
 */
/**
 * @ingroup LCI_HANDLER
 * @brief Create an active message handler.
 *
 * @param [in]  device     The device it should be associated to.
 * @param [in]  handler    The function handler to invoke when an operation
 * completes.
 * @param [out] completion The pointer to the completion queue to be created.
 * @return Should always be LCI_OK. All the other errors are fatal
 * as defined by @ref LCI_error_t.
 */
LCI_API
LCI_error_t LCI_handler_create(LCI_device_t device, LCI_handler_t handler,
                               LCI_comp_t* completion);
// progress
LCI_API
LCI_error_t LCI_progress(LCI_device_t device);
// memory management
LCI_API
LCI_error_t LCI_memory_register(LCI_device_t device, void* address,
                                size_t length, LCI_segment_t* segment);
LCI_API
LCI_error_t LCI_memory_deregister(LCI_segment_t* segment);
LCI_API
LCI_error_t LCI_mbuffer_alloc(LCI_device_t device, LCI_mbuffer_t* mbuffer);
LCI_API
LCI_error_t LCI_mbuffer_free(LCI_mbuffer_t mbuffer);
LCI_API
LCI_error_t LCI_lbuffer_alloc(LCI_device_t device, size_t size,
                              LCI_lbuffer_t* lbuffer);
LCI_API
LCI_error_t LCI_lbuffer_memalign(LCI_device_t device, size_t size,
                                 size_t alignment, LCI_lbuffer_t* lbuffer);
LCI_API
LCI_error_t LCI_lbuffer_free(LCI_lbuffer_t lbuffer);
// other helper functions
LCI_API
size_t LCI_get_iovec_piggy_back_size(int count);

#ifdef __cplusplus
}
#endif

#endif
