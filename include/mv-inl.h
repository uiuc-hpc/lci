#ifndef MV_INL_H
#define MV_INL_H

#include "mv.h"

/*! Packet manupulation functions */
inline void mv_pp_init(mv_pp**);
inline void mv_pp_destroy(mv_pp*);
inline void mv_pp_ext(mv_pp*, int nworker);
inline void mv_pp_free(mv_pp*, struct packet*);
inline void mv_pp_free_to(mv_pp*, struct packet*, int pid);
inline struct packet* mv_pp_alloc(mv_pp*, int pid);
inline struct packet* mv_pp_alloc_nb(mv_pp*, int pid);

/*! Hashtable functions */
inline void mv_hash_init(mv_hash** h);
inline int mv_hash_insert(mv_hash* h, mv_key key, mv_value* value);

/*! Server functions */
inline void mv_server_init(mv_server** s, mv_engine* mv, mv_pp*, int& rank,
                           int& size);
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
#include "packet_pool-inl.h"
#include "server/server.h"

#include "progress.h"
#include "proto.h"
#include "request.h"

MV_INLINE void* mv_heap_ptr(mv_engine* mv) {
  return mv_server_heap_ptr(mv->server);
}

MV_INLINE void mv_set_num_worker(mv_engine* mv, int number) {
  mv_pp_ext(mv->pkpool, number);
}

#endif
