#ifndef MPIV_MV_H_
#define MPIV_MV_H_

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include "macro.h"
#include "ult.h"

int mv_my_worker_id();

/*! Init context */
struct mv_struct;
typedef struct mv_struct mv_engine;
void mv_open(int *argc, char*** args, mv_engine**); 
void mv_close(mv_engine*);

/*! Memory function */
void* mv_malloc(mv_engine*, size_t size);
void mv_free(mv_engine*, void* ptr);
void mv_set_num_worker(mv_engine*, int number);

/*! Communication function */
struct MPIV_Request; 
void mv_send(mv_engine*, const void* buffer, size_t size, int rank, int tag);
void mv_recv(mv_engine*, void* buffer, size_t size, int rank, int tag);
void mv_isend(mv_engine*, const void* buffer, size_t size, int rank, int tag, MPIV_Request*);
void mv_irecv(mv_engine*, void* buffer, size_t size, int rank, int tag, MPIV_Request*);
void mv_waitall(mv_engine*, int count, MPIV_Request* req);

/*! Packet manupulation functions */
struct packet;
struct mv_pp;

inline void mv_pp_init(mv_pp**);
inline void mv_pp_destroy(mv_pp*);
inline void mv_pp_ext(mv_pp*, int nworker);
inline void mv_pp_free(mv_pp*, struct packet*);
inline void mv_pp_free_to(mv_pp*, struct packet*, int pid);
inline struct packet* mv_pp_alloc(mv_pp*, int pid);
inline struct packet* mv_pp_alloc_nb(mv_pp*, int pid);

/*! Hashtable functions */
typedef uintptr_t mv_value;
typedef uint64_t mv_key;
typedef void* mv_hash;

inline void mv_hash_init(mv_hash** h);
inline int mv_hash_insert(mv_hash* h, mv_key key, mv_value* value);

/*! Server functions */
typedef struct rdmax_server mv_server;
typedef void (*mv_am_func_t)();

inline void mv_server_init(mv_server** s, mv_engine* mv, mv_pp*, int& rank, int& size);
inline void mv_server_post_recv(mv_server* s, packet* p);
inline bool mv_server_progress(mv_server* s);
inline void mv_server_serve(mv_server* s);
inline void mv_server_send(mv_server* s, int rank, void* buf, size_t size, void* ctx);
inline void mv_server_rma(mv_server* s, int rank, void* from,
    uint32_t lkey, void* to, uint32_t rkey, size_t size, void* ctx);
inline void mv_server_rma_signal(mv_server *s, int rank, void* from,
    uint32_t lkey, void* to, uint32_t rkey, size_t size, uint32_t sid, void* ctx);
inline void* mv_server_alloc(mv_server* s, size_t size);
inline void mv_server_dealloc(mv_server* s, void* ptr);
inline void mv_server_finalize(mv_server* s);

/*! Progress functions */
void mv_serve_imm(uint32_t imm);
void mv_serve_recv(mv_engine*, packet*);
void mv_serve_send(mv_engine*, packet*);

struct mv_struct {
  int me;
  int size;
  mv_server* server;
  mv_hash* tbl;
  mv_pp* pkpool;
  std::vector<mv_am_func_t> am_table;
} __attribute__((aligned(64)));

#endif
