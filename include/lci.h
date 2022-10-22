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
  LCI_ERR_RETRY,      /* Resource temporarily not available. Try again. */
  LCI_ERR_FEATURE_NA, /* Feature not available */
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
  LCI_COMPLETION_QUEUE,  	// completion queue
  LCI_COMPLETION_HANDLER,  	// handler
  LCI_COMPLETION_SYNC, 	        // synchronizer
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
struct LCIS_mr_t;
typedef struct LCIS_mr_t *LCI_segment_t;

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
//typedef uint64_t LCI_short_t;

/**
 * LCI IOVEC.
 */
struct LCI_iovec_t {
  LCI_mbuffer_t piggy_back; // 16 bytes
  LCI_lbuffer_t *lbuffers;  // 8 bytes
  int count;                // 4 bytes
};
typedef struct LCI_iovec_t LCI_iovec_t; // 28 bytes

/**
 * The type of data associated with a buffer.
 * @todo should we add a flag to identify whether this buffer is allocated by users or LCI?
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
  LCI_error_t flag;       // 4 bytes
  int rank;               // 4 bytes
  LCI_tag_t tag;          // 4 bytes
  LCI_data_type_t type;   // 4 bytes
  LCI_data_t data;        // 32 bytes
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
typedef void (*LCI_handler_t)(LCI_request_t request);

/**@}*/

/**
 * \defgroup LCIGlobal LCI Global variables
 * @{
 */

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
 * maximum number of distinct memory segments that can be registered with a device.
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

extern LCI_device_t LCI_UR_DEVICE;
extern LCI_endpoint_t LCI_UR_ENDPOINT;
extern LCI_comp_t LCI_UR_CQ;
/**@}*/

// base
LCI_API
LCI_error_t LCI_initialize();
LCI_API
LCI_error_t LCI_initialized(int *flag);
LCI_API
LCI_error_t LCI_finalize();
LCI_API
LCI_error_t LCI_barrier();
// device
LCI_API
LCI_error_t LCI_device_init(LCI_device_t *device);
LCI_API
LCI_error_t LCI_device_free(LCI_device_t *device);
// plist
LCI_API
LCI_error_t LCI_plist_create(LCI_plist_t *plist);
LCI_API
LCI_error_t LCI_plist_free(LCI_plist_t *plist);
LCI_API
LCI_error_t LCI_plist_get(LCI_endpoint_t ep, LCI_plist_t *plist_ptr);
LCI_API
LCI_error_t LCI_plist_decode(LCI_plist_t plist, char *string);
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
LCI_error_t LCI_endpoint_free(LCI_endpoint_t *endpoint);
// two-sided functions
LCI_API
LCI_error_t LCI_sends(LCI_endpoint_t ep, LCI_short_t src, int rank, LCI_tag_t tag);
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
// Completion queue
LCI_API
LCI_error_t LCI_queue_create(LCI_device_t device, LCI_comp_t* cq);
LCI_API
LCI_error_t LCI_queue_free(LCI_comp_t* cq);
LCI_API
LCI_error_t LCI_queue_pop(LCI_comp_t cq, LCI_request_t* request);
LCI_API
LCI_error_t LCI_queue_wait(LCI_comp_t cq, LCI_request_t* request);
LCI_API
LCI_error_t LCI_queue_pop_multiple(LCI_comp_t cq, size_t request_count,
                                   LCI_request_t* requests,
                                   size_t* return_count);
LCI_API
LCI_error_t LCI_queue_wait_multiple(LCI_comp_t cq, size_t request_count,
                                    LCI_request_t* requests);
LCI_API
LCI_error_t LCI_queue_len(LCI_comp_t cq, size_t *len);
// synchronizer
LCI_API
LCI_error_t LCI_sync_create(LCI_device_t device, int threshold,
                            LCI_comp_t* sync);
LCI_API
LCI_error_t LCI_sync_free(LCI_comp_t *completion);
LCI_API
LCI_error_t LCI_sync_signal(LCI_comp_t completion, LCI_request_t request);
LCI_API
LCI_error_t LCI_sync_wait(LCI_comp_t sync, LCI_request_t request[]);
LCI_API
LCI_error_t LCI_sync_test(LCI_comp_t completion, LCI_request_t request[]);
// handler
LCI_API
LCI_error_t LCI_handler_create(LCI_device_t device, LCI_handler_t handler,
                               LCI_comp_t *completion);
// progress
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
// other helper functions
LCI_API
size_t LCI_get_iovec_piggy_back_size(int count);

#ifdef __cplusplus
}
#endif

#endif
