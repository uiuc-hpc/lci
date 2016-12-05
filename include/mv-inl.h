#ifndef MV_INL_H
#define MV_INL_H

#include "mv.h"

/*! Hashtable functions */
inline void mv_hash_init(mv_hash** h);
inline int mv_hash_insert(mv_hash* h, mv_key key, mv_value* value);

/*! Server functions */
inline void mv_server_init(mv_server** s, mv_engine* mv, int& rank, int& size);
inline void mv_server_post_recv(mv_server* s, packet* p);
inline void mv_server_serve(mv_server* s);
inline void mv_server_send(mv_server* s, int rank, void* buf, size_t size,
                           void* ctx);
inline void mv_server_rma(mv_server* s, int rank, void* from, uint32_t lkey,
                          void* to, uint32_t rkey, size_t size, void* ctx);
inline void mv_server_rma_signal(mv_server* s, int rank, void* from,
                                 uint32_t lkey, void* to, uint32_t rkey,
                                 size_t size, uint32_t sid, void* ctx);
inline void mv_server_finalize(mv_server* s);

/*! Progress functions */
inline void mv_serve_imm(uint32_t imm);
inline void mv_serve_recv(mv_engine*, packet*);
inline void mv_serve_send(mv_engine*, packet*);

#include "common.h"
#include "hashtable-inl.h"
#include "packet.h"
#include "server/server.h"

#include "progress.h"
#include "proto.h"
#include "request.h"

MV_INLINE void* mv_heap_ptr(mv_engine* mv)
{
  return mv_server_heap_ptr(mv->server);
}

#endif
