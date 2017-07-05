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
#include <stdint.h>
#include <stdlib.h>

#define LC_EXPORT __attribute__((visibility("default")))

/*! Init context */
struct lc_struct;
typedef struct lc_struct lch;

typedef void (*lc_am_func_t)();
struct lc_ctx;
typedef struct lc_ctx lc_ctx;

struct lc_packet;
typedef struct lc_packet lc_packet;
struct lc_pool;
typedef struct lc_pool lc_pool;

typedef uintptr_t lc_value;
typedef uint64_t lc_key;
typedef void* lc_hash;

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

typedef int (*lc_fcb)(lch* mv, lc_ctx* ctx, lc_sync* sync);

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
  lc_fcb complete;
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
int lc_send_tag(lch* mv, const void* src, int size, int rank, int tag, lc_ctx* ctx);

/**
* @brief Try to finish send, or insert sync for waking up.
*
* @param mv
* @param ctx
* @param sync
*
* @return 1 if finished, 0 otherwise.
*
*/
LC_EXPORT
int lc_send_tag_post(lch* mv, lc_ctx* ctx, lc_sync* sync);

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
int lc_recv_tag(lch* mv, void* src, int size, int rank, int tag, lc_ctx* ctx);

/**
* @brief Try to match and insert sync obj for waking up.
*
* @param mv
* @param ctx
* @param sync
*
* @return 1 if finished, 0 otherwise.
*/
LC_EXPORT
int lc_recv_tag_post(lch* mv, lc_ctx* ctx, lc_sync* sync);

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
int lc_send_queue(lch* mv, const void* src, int size, int rank, int tag,
                  lc_ctx* ctx);

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
int lc_recv_queue(lch* mv, int* size, int* rank, int* tag, lc_ctx* ctx);

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
int lc_recv_queue_post(lch* mv, void* buf, lc_ctx* ctx);

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
                lc_ctx* ctx);

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
int lc_recv_put_signal(lch* mv, lc_addr* rctx, lc_ctx* ctx);

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
                       lc_ctx* ctx);

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
* @param sync
*
* @return
*/
LC_INLINE
void lc_wait(lc_ctx* ctx, lc_sync* sync)
{
  while (ctx->type != REQ_DONE) {
    thread_wait(sync);
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
int lc_test(lc_ctx* ctx) { return (ctx->type == REQ_DONE); }

/**@} end control */

/**@} end low-level */

/**
* @defgroup exp-api Experimental API
* @{
*/

LC_EXPORT
void* lc_heap_ptr(lch* mv);

LC_EXPORT
uint8_t lc_am_register(lch* mv, lc_am_func_t f);

LC_EXPORT
int lc_progress(lch* mv);

LC_EXPORT
lc_packet* lc_alloc_packet(lch* mv, int size);

LC_EXPORT
void* lc_get_packet_data(lc_packet* p);

LC_EXPORT
void lc_free_packet(lch* mv, lc_packet* p);

LC_EXPORT
void lc_send_persis(lch* mv, lc_packet* p, int rank, int tag, lc_ctx* ctx);

LC_EXPORT
int lc_id(lch* mv);

LC_EXPORT
int lc_size(lch* mv);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif
