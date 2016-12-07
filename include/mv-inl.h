#ifndef MV_INL_H
#define MV_INL_H

#include "mv.h"

/*! Hashtable functions */
void mv_hash_init(mv_hash** h);
int mv_hash_insert(mv_hash* h, mv_key key, mv_value* value);

/*! Server functions */
#if 0
MV_INLINE void mv_server_init(mv_server** s, mv_engine* mv, int& rank, int& size);
MV_INLINE void mv_server_post_recv(mv_server* s, mv_packet* p);
MV_INLINE void mv_server_serve(mv_server* s);
MV_INLINE void mv_server_send(mv_server* s, int rank, void* buf, size_t size,
                           void* ctx);
MV_INLINE void mv_server_rma(mv_server* s, int rank, void* from, uint32_t lkey,
                          void* to, uint32_t rkey, size_t size, void* ctx);
MV_INLINE void mv_server_rma_signal(mv_server* s, int rank, void* from,
                                 uint32_t lkey, void* to, uint32_t rkey,
                                 size_t size, uint32_t sid, void* ctx);
MV_INLINE void mv_server_finalize(mv_server* s);
#endif

void mv_serve_recv(mv_engine* mv, mv_packet* p_ctx);
void mv_serve_send(mv_engine* mv, mv_packet* p_ctx);
void mv_serve_imm(uint32_t imm);

#include "common.h"
#include "packet.h"
#include "server/server.h"

#include "progress.h"
#include "proto.h"
#include "request.h"

#endif
