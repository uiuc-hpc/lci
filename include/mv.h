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

  MV_PROTO_GENERIC,

  MV_PROTO_SHORT_ENQUEUE,
  MV_PROTO_RTS_ENQUEUE,
  MV_PROTO_RTR_ENQUEUE,
  MV_PROTO_LONG_ENQUEUE,
};

struct __attribute__((__packed__)) packet_header {
  enum mv_proto_name proto __attribute__((aligned(4)));
  int from;
  int tag;
  int size;
};

struct __attribute__((__packed__)) mv_rdz {
  uintptr_t sreq;
  uintptr_t tgt_addr;
  uint32_t rkey;
  uint32_t comm_id;
};

union packet_content {
  struct mv_rdz rdz;
  char buffer[0];
};

struct __attribute__((__packed__)) packet_data {
  struct packet_header header;
  union packet_content content;
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
int mv_send_init(mvh* mv, const void* src, int size, int rank, int tag, mv_ctx* ctx);

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
int mv_recv_init(mvh* mv, void* src, int size, int rank, int tag, mv_ctx* ctx);

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
* @brief Initialize a rdz send, enqueue at destination.
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
int mv_send_enqueue_init(mvh* mv, const void* src, int size, int rank, int tag,
                         mv_ctx* ctx);

/**
* @brief Try to finish or attach sync for waking up.
*
* @param mv
* @param ctx
* @param sync
*
* @return 1 if finished, 0 otherwise.
*/
MV_EXPORT
int mv_send_enqueue_post(mvh* mv, mv_ctx* ctx, mv_sync* sync);

/**
* @brief Try to dequeue, for message send with send-enqueue.
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
int mv_recv_dequeue_init(mvh* mv, int *size, int *rank, int *tag, mv_ctx* ctx);

/**
* @brief Try to finish a dequeue op, for message send with send-enqueue.
*
* @param mv
* @param buf An allocated buffer (with mv_alloc).
* @param ctx
*
* @return 1 if finished, 0 otherwise.
*/
MV_EXPORT
int mv_recv_dequeue_post(mvh* mv, void* buf, mv_ctx* ctx);

/**@} End queue group */

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
* @brief Blocking wait on sync, for communication to finish
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
int mv_test(mv_ctx* ctx) { return (ctx->type == REQ_DONE); }
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
void mv_am_eager(mvh* mv, int node, void* src, int size, int tag, uint8_t fid);

MV_EXPORT
void mv_am_eager2(mvh* mv, int node, void* src, int size, int tag,
                  uint8_t am_fid, uint8_t ps_fid, mv_packet* p);

MV_EXPORT
void mv_put(mvh* mv, int node, void* dst, void* src, int size);

MV_EXPORT
void mv_put_signal(mvh* mv, int node, void* dst, void* src, int size,
                   uint32_t sid);

MV_EXPORT
void* mv_heap_ptr(mvh* mv);

MV_EXPORT
uint8_t mv_am_register(mvh* mv, mv_am_func_t f);

MV_EXPORT
void mv_progress(mvh* mv);

MV_EXPORT
size_t mv_data_max_size();

MV_EXPORT
size_t get_ncores();

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

/**@}*/

#ifdef __cplusplus
}
#endif

#endif
