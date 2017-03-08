/**
 * @file mv.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 * @brief Header file for all MPIv code.
 *
 */

#ifndef MPIV_MV_H_
#define MPIV_MV_H_

#include "mpi.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "mv/ult/ult.h"
#include <stdint.h>
#include <stdlib.h>

#define MV_EXPORT __attribute__((visibility("default")))

/*! Init context */
struct mv_struct;
typedef struct mv_struct mvh;

typedef void (*mv_am_func_t)();
struct mv_ctx;
typedef struct mv_ctx mv_ctx;

struct mv_packet;
typedef struct mv_packet mv_packet;
struct mv_pool;
typedef struct mv_pool mv_pool;

typedef uintptr_t mv_value;
typedef uint64_t mv_key;
typedef void* mv_hash;

// Keep this order, or change mv_proto.
enum mv_proto_name {
  MV_PROTO_NULL = 0,
  MV_PROTO_SHORT_MATCH,
  MV_PROTO_SHORT_WAIT,
  MV_PROTO_RTR_MATCH,
  MV_PROTO_LONG_MATCH,

  MV_PROTO_SHORT_QUEUE,
  MV_PROTO_RTS_QUEUE,
  MV_PROTO_RTR_QUEUE,
  MV_PROTO_LONG_QUEUE,

  MV_PROTO_LONG_PUT,
  MV_PROTO_PERSIS
};

typedef int (*mv_fcb)(mvh* mv, mv_ctx* ctx, mv_sync* sync);

enum request_t {
  REQ_NULL = 0,
  REQ_DONE,
  REQ_PENDING,
};

struct mv_ctx {
  void* buffer;
  int size;
  int rank;
  int tag;
  mv_sync* sync;
  mv_packet* packet;
  volatile enum request_t type;
  mv_fcb complete;
} __attribute__((aligned(64)));

struct mv_rma_ctx {
  uintptr_t req;
  uint64_t addr;
  uint32_t rkey;
  uint32_t sid;
} __attribute__((aligned(64)));

typedef struct mv_rma_ctx mv_addr;

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
MV_EXPORT
int mv_send(mvh* mv, const void* src, int size, int rank, int tag, mv_ctx* ctx);

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
MV_EXPORT
int mv_send_post(mvh* mv, mv_ctx* ctx, mv_sync* sync);

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
MV_EXPORT
int mv_recv(mvh* mv, void* src, int size, int rank, int tag, mv_ctx* ctx);

/**
* @brief Try to match and insert sync obj for waking up.
*
* @param mv
* @param ctx
* @param sync
*
* @return 1 if finished, 0 otherwise.
*/
MV_EXPORT
int mv_recv_post(mvh* mv, mv_ctx* ctx, mv_sync* sync);

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
MV_EXPORT
int mv_send_queue(mvh* mv, const void* src, int size, int rank, int tag,
                  mv_ctx* ctx);

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
MV_EXPORT
int mv_recv_queue(mvh* mv, int* size, int* rank, int* tag, mv_ctx* ctx);

/**
* @brief Try to finish a queue op, for message send with send-queue.
*
* @param mv
* @param buf An allocated buffer (with mv_alloc).
* @param ctx
*
* @return 1 if finished, 0 otherwise.
*/
MV_EXPORT
int mv_recv_queue_post(mvh* mv, void* buf, mv_ctx* ctx);

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
MV_EXPORT
int mv_rma_create(mvh* mv, void* buf, size_t size, mv_addr** rctx_ptr);

/**
* @brief performs an RDMA PUT to dst (created by mv_rma_create).
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
MV_EXPORT
int mv_send_put(mvh* mv, void* src, int size, int rank, mv_addr* dst,
                mv_ctx* ctx);

/**
* @brief assign a ctx to an rma handle for receiving signal.
*
* @param mv
* @param rctx
* @param ctx
*
* @return
*/
MV_EXPORT
int mv_recv_put_signal(mvh* mv, mv_addr* rctx, mv_ctx* ctx);

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
MV_EXPORT
int mv_send_put_signal(mvh* mv, void* src, int size, int rank, mv_addr* dst,
                       mv_ctx* ctx);

/**@} end rdma-api */


/**
 * @defgroup control Control APIs
 * @{
 */

/**
* @brief Create a handle for communication.
*
* @param argc
* @param args
* @param heap_size
* @param handle
*
*/
MV_EXPORT
void mv_open(int* argc, char*** args, size_t heap_size, mvh** handle);

/**
* @brief Close the handle and free memory.
*
* @param handle
*
*/
MV_EXPORT
void mv_close(mvh* handle);

/**
* @brief Blocking wait on sync, for communication to finish.
*
* @param ctx
* @param sync
*
* @return
*/
MV_INLINE
void mv_wait(mv_ctx* ctx, mv_sync* sync)
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
MV_INLINE
int mv_test(mv_ctx* ctx)
{
  return (ctx->type == REQ_DONE);
}

/**
* @brief Allocate memory for communication
*
* @param mv
* @param size
*
* @return buffer
*/
MV_EXPORT
void* mv_alloc(size_t size);

/**
* @brief Free allocated memory.
*
* @param mv
* @param buffer
*/
MV_EXPORT
void mv_free(void* buffer);

/**@} end control */

/**@} end low-level */

/**
* @defgroup exp-api Experimental API
* @{
*/

MV_EXPORT
void* mv_heap_ptr(mvh* mv);

MV_EXPORT
uint8_t mv_am_register(mvh* mv, mv_am_func_t f);

MV_EXPORT
void mv_progress(mvh* mv);

MV_EXPORT
size_t mv_get_ncores();

/**@}*/

/**
* @defgroup mpi MPI-like API
*	@{
*/
typedef uintptr_t MPIV_Request;
extern mvh* mv_hdl;

MV_EXPORT
void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm, MPI_Status*);

MV_EXPORT
void MPIV_Send(void* buffer, int count, MPI_Datatype datatype, int rank,
               int tag, MPI_Comm);

MV_EXPORT
void MPIV_Ssend(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm);

MV_EXPORT
void MPIV_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req);

MV_EXPORT
void MPIV_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm, MPIV_Request* req);

MV_EXPORT
void MPIV_Waitall(int count, MPIV_Request* req, MPI_Status*);

MV_EXPORT
void MPIV_Init(int* argc, char*** args);

MV_EXPORT
void MPIV_Finalize();

MV_EXPORT
void* MPIV_Alloc(size_t size);

MV_EXPORT
void MPIV_Free(void*);

MV_EXPORT
mv_packet* mv_alloc_packet(mvh* mv, int tag, int size);

MV_EXPORT
void* mv_get_packet_data(mv_packet* p);

MV_EXPORT
void mv_free_packet(mvh* mv, mv_packet* p);

MV_EXPORT
void mv_send_persis(mvh* mv, mv_packet* p, int rank, mv_ctx* ctx);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif
