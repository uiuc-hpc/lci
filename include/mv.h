#ifndef MPIV_MV_H_
#define MPIV_MV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "ult/ult.h"
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

#if defined(MV_USE_SERVER_IBV)
typedef struct ibv_server mv_server;
#elif defined(MV_USE_SERVER_OFI)
typedef struct ofi_server mv_server;
#else
#error("Need server definition (MV_USE_SERVER_IBV | MV_USE_SERVER_OFI)")
#endif

typedef uintptr_t mv_value;
typedef uint64_t mv_key;
typedef void* mv_hash;

MV_EXPORT
void mv_open(int* argc, char*** args, size_t heap_size, mvh**);

MV_EXPORT
void mv_close(mvh*);

MV_EXPORT
void mv_send_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync);

MV_EXPORT
void mv_send_rdz_init(mvh* mv, mv_ctx* ctx);

MV_EXPORT
void mv_send_rdz(mvh* mv, mv_ctx* ctx, mv_sync* sync);

MV_EXPORT
void mv_send_eager(mvh* mv, mv_ctx* ctx);

MV_EXPORT
void mv_recv_rdz(mvh* mv, mv_ctx* ctx, mv_sync* sync);

MV_EXPORT
void mv_recv_eager(mvh* mv, mv_ctx* ctx, mv_sync* sync);

MV_EXPORT
void mv_am_eager(mvh* mv, int node, void* src, int size,
    uint32_t fid);

MV_EXPORT
void mv_put(mvh* mv, int node, void* dst, void* src, int size,
    uint32_t sid);

MV_EXPORT
void* mv_heap_ptr(mvh* mv);

MV_EXPORT
uint8_t mv_am_register(mvh* mv, mv_am_func_t f);

/*! MPI like functions */
#include <mpi.h>
typedef uintptr_t MPIV_Request;
extern mvh* mv_hdl;

MV_EXPORT
void MPIV_Recv(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag, MPI_Comm, MPI_Status*);

MV_EXPORT
void MPIV_Send(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag, MPI_Comm);

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

void mv_recv_rdz_init(mvh* mv, mv_ctx* ctx);
void mv_recv_rdz_post(mvh* mv, mv_ctx* ctx, mv_sync* sync);
void mv_recv_eager_post(mvh* mv, mv_ctx* ctx, mv_sync* sync);

#ifdef __cplusplus
}
#endif

#endif
