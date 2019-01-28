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
 * LCI Status type.
 */
typedef enum LCI_Status {
  LCI_OK = 0,
  LCI_ERR_RETRY,
  LCI_ERR_FATAL,
} LCI_Status;

/**
 * LCI Communication type.
 */
typedef enum LCI_Comm_type {
  LCI_CT_1SIDED,
  LCI_CT_2SIDED,
  LCI_CT_COLL,
} LCI_Comm_type;

/**
 * LCI Message type.
 */
typedef enum LCI_Message_type {
  LCI_MT_SHORT,
  LCI_MT_MEDIUM,
  LCI_MT_LONG,
} LCI_Message_type;

/**
 * LCI Synchronizer type.
 */
typedef enum LCI_Sync_type {
  LCI_ST_SYNC,
  LCI_ST_CQ,
  LCI_ST_AM,
} LCI_Sync_type;

/**
 * Synchronizer object, owned by the runtime.
 */
struct LCI_Sync_s;
typedef struct LCI_Sync_s* LCI_Sync;

/**
 * Request object, owned by the user, unless returned from runtime (CQ_Dequeue).
 */
typedef struct LCI_Request_s {
  void* buffer;
  size_t size;
  int rank;
  int tag;

  /* Addition data can be stored here. */
  void* usr_ctx;

  /* Status of the communication. */
  LCI_Status status;

  /* Pointer to the sync object (if there is). */
  struct LCI_Sync_s* sync;

  /* Reserved by the implementation. */
  void* __reserved__;
} LCI_Request;

/**
 * Endpoint object, owned by the runtime.
 */
struct LCI_Endpoint_s;
typedef struct LCI_Endpoint_s* LCI_Endpoint;

/**
 * Property object, owned by the runtime.
 */
struct LCI_Property_s;
typedef struct LCI_Property_s* LCI_Property;

/**
 * Completion queue object, owned by the runtime.
 */
struct LCI_Cq_s;
typedef struct LCI_Cq_s* LCI_Cq;

/**
 * Handler type
 */
typedef void (*LCI_Handler)(LCI_Request* req, void* usr_context);

/**
 * Allocator type
 */
typedef void* (*LCI_Allocator)(size_t size, void* usr_context);

/**
 * Initialize LCI.
 */
LCI_API
LCI_Status LCI_Initialize(int num_devices);

/**
 * Finalize LCI.
 */
LCI_API
LCI_Status LCI_Finalize();

/**
 * Create an Endpoint Property @prop.
 */
LCI_API
LCI_Status LCI_Property_create(LCI_Property* prop);

/**
 * Set communication style (1sided, 2sided, collective).
 */
LCI_API
LCI_Status LCI_Property_set_comm_type(LCI_Comm_type type, LCI_Property* prop);

/**
 * Set message type (short, medium, long).
 */
LCI_API
LCI_Status LCI_Property_set_message_type(LCI_Message_type type, LCI_Property* prop);

/**
 * Set synchronization type for completion.
 */
LCI_API
LCI_Status LCI_Property_set_sync_type(LCI_Sync_type ltype, LCI_Sync_type rtype, LCI_Property* prop);

/**
 * Set handler for AM protocol.
 */
LCI_API
LCI_Status LCI_Property_set_handler(LCI_Handler handler, LCI_Property* prop);

/**
 * Set allocator for dynamic protocol.
 */
LCI_API
LCI_Status LCI_Property_set_allocator(LCI_Allocator alloc, LCI_Property* prop);

/**
 * Create an Endpoint, collective calls for those involved in the endpoints.
 */
LCI_API
LCI_Status LCI_Endpoint_create(int device_id, LCI_Property prop, LCI_Endpoint* ep);

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

/* Two-sided functions. */

/**
 * Send a short message, completed immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_Status LCI_Sends(void* src, size_t size, int rank, int tag, LCI_Endpoint ep);

/**
 * Send a medium message, completed immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_Status LCI_Sendm(void* src, size_t size, int rank, int tag, LCI_Endpoint ep);

/**
 * Send a long message.
 */
LCI_API
LCI_Status LCI_Sendl(void* src, size_t size, int rank, int tag, LCI_Endpoint ep, void* sync_context);

/**
 * Receive a short message.
 */
LCI_API
LCI_Status LCI_Recvs(void* src, size_t size, int rank, int tag, LCI_Endpoint ep, void* sync_context, LCI_Request* req);

/**
 * Receive a medium message.
 */
LCI_API
LCI_Status LCI_Recvm(void* src, size_t size, int rank, int tag, LCI_Endpoint ep, void* sync_context, LCI_Request* req);

/**
 * Receive a medium message.
 */
LCI_API
LCI_Status LCI_Recvl(void* src, size_t size, int rank, int tag, LCI_Endpoint ep, void* sync_context, LCI_Request* req);

/* One-sided functions. */

/**
 * Put short message to a remote address @rma_id available at the remote endpoint, offset @offset.
 * Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_Status LCI_Puts(void* src, size_t size, int rank, int rma_id, int offset, LCI_Endpoint ep);

/**
 * Put medium message to a remote address @rma_id available at the remote endpoint, offset @offset.
 * Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_Status LCI_Putm(void* src, size_t size, int rank, int rma_id, int offset, LCI_Endpoint ep);

/**
 * Put long message to a remote address @rma_id available at the remote endpoint, offset @offset.
 */
LCI_API
LCI_Status LCI_Putl(void* src, size_t size, int rank, int rma_id, int offset, LCI_Endpoint ep, void* context);

/**
 * Put short message to a remote address, piggy-back data to completed request.
 * Complete immediately or LCI_ERR_RETRY.
 */
LCI_API
LCI_Status LCI_Putsd(void* src, size_t size, int rank, int tag, LCI_Endpoint ep);

/**
 * Put medium message to a remote address, piggy-back data to completed request.
 * Complete immediately or LCI_ERR_RETRY.
 */
LCI_API
LCI_Status LCI_Putmd(void* src, size_t size, int rank, int tag, LCI_Endpoint ep);

/**
 * Put long message to a remote address, required a remote allocation.
 */
LCI_API
LCI_Status LCI_Putld(void* src, size_t size, int rank, int tag, LCI_Endpoint ep, void* sync_context, LCI_Request* request);

/* Completion methods */

/**
 * Create a completion queue.
 */
LCI_API
LCI_Status LCI_Cq_create(LCI_Cq* ep);

/**
 * Return first completed request in the queue.
 */
LCI_API
LCI_Status LCI_Cq_dequeue(LCI_Cq ep, LCI_Request** req);

/**
 * Return at most @n first completed request in the queue.
 */
LCI_API
LCI_Status LCI_Cq_mul_dequeue(LCI_Cq ep, int n, int* actual, LCI_Request** req);

/**
 * Return @n requests to the runtime.
 */
LCI_API
LCI_Status LCI_Request_free(int n, LCI_Request** req);

/**
 * Create a Sync object.
 */
LCI_API
LCI_Status LCI_Sync_create(LCI_Sync* sync);

/**
 * Reset on a Sync object.
 */
LCI_API
LCI_Status LCI_Sync_reset(LCI_Sync* sync);

/**
 * Wait on a Sync object.
 */
LCI_API
LCI_Status LCI_Sync_wait(LCI_Sync sync);

/**
 * Test a Sync object, return 1 if finished.
 */
LCI_API
int LCI_Sync_test(LCI_Sync sync);

/**
 * Signal a Sync object.
 */
LCI_API
LCI_Status LCI_Sync_signal(LCI_Sync sync);

/**
 * Polling a specific device @device_id for at least @count time.
 */
LCI_API
LCI_Status LCI_Progress(int device_id, int count);

#ifdef __cplusplus
}
#endif

#endif
