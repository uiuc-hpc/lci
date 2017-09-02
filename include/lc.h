/**
 * @file mv.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 * @brief Header file for all MPIv code.
 *
 */

#ifndef LC_H_
#define LC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "thread.h"
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

typedef void (*lc_fcb)(lch* mv, lc_req* req, lc_packet* p);

enum lc_req_state {
  LC_REQ_PEND = 0,
  LC_REQ_DONE = 1,
};

struct lc_ctx {
  void* buffer;
  int size;
  int rank;
  int tag;
  union {
    volatile enum lc_req_state type;
    volatile int int_type;
  };
  lc_sync* sync;
  lc_packet* packet;
  lc_fcb finalize;
} __attribute__((aligned(64)));

struct lc_rma_ctx {
  uintptr_t req;
  uint64_t addr;
  uint32_t rkey;
  uint32_t sid;
} __attribute__((aligned(64)));

typedef struct lc_rma_ctx lc_addr;

struct lc_pkt {
  void* _reserved_;
  void* buffer;
  int rank;
  int tag;
};

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
* @param ctx
*
* @return 1 if success, 0 otherwise -- need to retry.
*
*/
LC_EXPORT
lc_status lc_send_tag(lch* mv, const void* src, int size, int rank, int tag,
                      lc_req* ctx);

/**
* @brief Send a buffer and match at destination (packetized version)
*
* @param mv
* @param pkt
* @param ctx
*
* @return 1 if success, 0 otherwise -- need to retry.
*
*/
LC_EXPORT
lc_status lc_send_tag_p(lch* mv, struct lc_pkt* pkt, lc_req* ctx);

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
lc_status lc_recv_tag(lch* mv, void* src, int size, int rank, int tag,
                      lc_req* ctx);

/**@} End group matching */

/**
 * @defgroup queue Enqueue/Dequeue
 * @{
 */

/**
* @brief Initialize a send, queue at destination.
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
* @brief Initialize a send, queue at destination (packetized version).
*
* @param mv
* @param pkt
* @param ctx
*
* @return 1 if success, 0 otherwise -- need to retry.
*
*/
LC_EXPORT
lc_status lc_send_queue_p(lch* mv, struct lc_pkt* pkt, lc_req* ctx);

/**
* @brief Try to dequeue, for message send with send-queue.
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
lc_status lc_recv_queue_probe(lch* mv, int* size, int* rank, int* tag,
                              lc_req* ctx);

/**
* @brief Try to finish a queue op, for message send with send-queue.
*
* @param mv
* @param buf An allocated buffer.
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

LC_INLINE
lc_status lc_post(lc_req* ctx, lc_sync* sync)
{
  if (ctx->type == LC_REQ_DONE) {
    return LC_OK;
  } else {
    if (__sync_val_compare_and_swap(&ctx->sync, NULL, sync) == NULL) {
      return LC_ERR_NOP;
    } else {
      return LC_OK;
    }
  }
}

/**
* @brief Blocking wait on sync, for communication to finish.
*
* @param ctx
*
* @return
*/
LC_INLINE
void lc_wait(lch* mv __UNUSED__, lc_req* ctx, lc_sync* sync)
{
  if (lc_post(ctx, sync) == LC_ERR_NOP) {
    lc_sync_wait(ctx->sync, &(ctx->int_type));
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
int lc_test(lc_req* ctx) { return (ctx->type == LC_REQ_DONE); }
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
lc_status lc_pkt_init(lch* mv, int size, int rank, int tag, struct lc_pkt*);

LC_EXPORT
void lc_pkt_fini(lch* mv, struct lc_pkt* p);

LC_EXPORT
int lc_id(lch* mv);

LC_EXPORT
int lc_size(lch* mv);

LC_INLINE
void lc_wait_poll(lch* mv, lc_req* ctx)
{
  while (!lc_test(ctx)) lc_progress(mv);
}

LC_EXPORT
void lc_sync_init(lc_get_fp i, lc_wait_fp w, lc_signal_fp s, lc_yield_fp y);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif
