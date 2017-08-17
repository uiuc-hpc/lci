/**
 * @file mv.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 * @brief Header file for all MPIv code.
 *
 */

#ifndef MPIV_LC_H_
#define MPIV_LC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ult/ult.h"
#include "ptmalloc.h"
#include <stdint.h>
#include <stdlib.h>

/*! Init context */
struct lc_struct;
typedef struct lc_struct lch;

typedef void (*lc_am_func_t)();
struct lc_ctx;
typedef struct lc_ctx lc_req;

struct lc_packet;
typedef struct lc_packet lc_packet;

typedef enum lc_status {
  LC_ERR_NOP = 0,
  LC_OK = 1,
} lc_status;

// Keep this order, or change lc_proto.
enum lc_proto_name {
  LC_PROTO_NULL = 0,
  LC_PROTO_SHORT_MATCH,
  LC_PROTO_RTR_MATCH,
  LC_PROTO_LONG_MATCH,

  LC_PROTO_SHORT_QUEUE,
  LC_PROTO_RTS_QUEUE,
  LC_PROTO_RTR_QUEUE,
  LC_PROTO_LONG_QUEUE,

  LC_PROTO_LONG_PUT,
  LC_PROTO_PERSIS
};

typedef lc_status (*lc_fcb)(lch* mv, lc_req* ctx, lc_sync* sync);

#define REQ_NULL 0
#define REQ_DONE 1
#define REQ_PENDING 2

struct lc_ctx {
  void* buffer;
  int size;
  int rank;
  int tag;
  volatile int type;
  lc_sync* sync;
  lc_packet* packet;
  lc_fcb post;
} __attribute__((aligned(64)));

struct lc_rma_ctx {
  uintptr_t req;
  uint64_t addr;
  uint32_t rkey;
  uint32_t sid;
} __attribute__((aligned(64)));

typedef struct lc_rma_ctx lc_addr;

/**
 * @defgroup low-level Low-level API
 * @{
 */

/**
 * @defgroup match Matching using rank, tag.
 * @{
 */

/**
* @brief Send a buffer and match at destination.
*
* @param mv
* @param src
* @param size
* @param rank
* @param tag
*
* @return 1 if success, 0 otherwise -- need to retry.
*
*/
LC_EXPORT
lc_status lc_send_tag(lch* mv, const void* src, int size, int rank, int tag, lc_req* ctx);

/**
* @brief Initialize a recv, matching incoming message.
*
* @param mv
* @param src
* @param size
* @param rank
* @param tag
* @param ctx
*
* @return 1 if success, 0 otherwise -- need to retry.
*
*/
LC_EXPORT
lc_status lc_recv_tag(lch* mv, void* src, int size, int rank, int tag, lc_req* ctx);

/**@} End group matching */

/**
 * @defgroup queue Enqueue/Dequeue
 * @{
 */

/**
* @brief Initialize a rdz send, queue at destination.
*
* @param mv
* @param src
* @param size
* @param rank
* @param tag
* @param ctx
*
* @return 1 if success, 0 otherwise -- need to retry.
*
*/
LC_EXPORT
lc_status lc_send_queue(lch* mv, const void* src, int size, int rank, int tag,
                        lc_req* ctx);

/**
* @brief Try to queue, for message send with send-queue.
*
* @param mv
* @param size
* @param rank
* @param tag
* @param ctx
*
* @return 1 if got data, 0 otherwise.
*/
LC_EXPORT
lc_status lc_recv_queue_probe(lch* mv, int* size, int* rank, int* tag, lc_req* ctx);

/**
* @brief Try to finish a queue op, for message send with send-queue.
*
* @param mv
* @param buf An allocated buffer (with lc_alloc).
* @param ctx
*
* @return 1 if finished, 0 otherwise.
*/
LC_EXPORT
lc_status lc_recv_queue(lch* mv, void* buf, lc_req* ctx);

/**@} End queue group */

/**
* @defgroup rdma-api API for one-sided communication
* @{
*/

/**
* @brief create an address for RMA.
*
* @param mv
* @param buf
* @param size
* @param rctx_ptr
*
* @return
*/
LC_EXPORT
int lc_rma_create(lch* mv, void* buf, size_t size, lc_addr** rctx_ptr);

/**
* @brief performs an RDMA PUT to dst (created by lc_rma_create).
*
* @param mv
* @param src
* @param size
* @param rank
* @param dst
* @param ctx
*
* @return 1 if success, 0 otherwise.
*/
LC_EXPORT
int lc_send_put(lch* mv, void* src, int size, int rank, lc_addr* dst,
                lc_req* ctx);

/**
* @brief assign a ctx to an rma handle for receiving signal.
*
* @param mv
* @param rctx
* @param ctx
*
* @return
*/
LC_EXPORT
int lc_recv_put_signal(lch* mv, lc_addr* rctx, lc_req* ctx);

/**
* @brief  performs an RDMA PUT to dst, and also signal a handle.
*
* @param mv
* @param src
* @param size
* @param rank
* @param dst
* @param ctx
*
* @return
*/
LC_EXPORT
int lc_send_put_signal(lch* mv, void* src, int size, int rank, lc_addr* dst,
                       lc_req* ctx);

/**@} end rdma-api */

/**
 * @defgroup control Control APIs
 * @{
 */

/**
* @brief Create a handle for communication.
*
* @param handle
*
*/
LC_EXPORT
void lc_open(lch** handle);

/**
* @brief Close the handle and free memory.
*
* @param handle
*
*/
LC_EXPORT
void lc_close(lch* handle);

/**
* @brief Blocking wait on sync, for communication to finish.
*
* @param ctx
*
* @return
*/
LC_INLINE
void lc_wait(lc_req* ctx)
{
  while (ctx->type != REQ_DONE) {
    lc_thread_wait(ctx->sync);
  }
}

/**
* @brief Non-blocking test if the communication is finished.
*
* @param ctx
*
* @return 1 if finished, 0 otherwise.
*/
LC_INLINE
int lc_test(lc_req* ctx) {
  return (ctx->type == REQ_DONE);
}

/**@} end control */

/**@} end low-level */

/**
* @defgroup exp-api Experimental API
* @{
*/

LC_EXPORT
void* lc_heap_ptr(lch* mv);

LC_EXPORT
int lc_progress(lch* mv);

LC_EXPORT
lc_packet* lc_alloc_packet(lch* mv, int size);

LC_EXPORT
void* lc_get_packet_data(lc_packet* p);

LC_EXPORT
void lc_free_packet(lch* mv, lc_packet* p);

LC_EXPORT
void lc_send_persis(lch* mv, lc_packet* p, int rank, int tag, lc_req* ctx);

LC_EXPORT
int lc_id(lch* mv);

LC_EXPORT
int lc_size(lch* mv);

LC_INLINE
lc_status lc_post(lch* mv, lc_req* ctx, lc_sync* sync)
{
  if (ctx->post)
    return ctx->post(mv, ctx, sync);
  else
    return LC_OK;
}

LC_INLINE
void lc_wait_poll(lch* mv, lc_req* ctx) {
  lc_post(mv, ctx, NULL);
  while (!lc_test(ctx))
    lc_progress(mv);
}
/**@}*/

#ifdef __cplusplus
}
#endif

#endif
