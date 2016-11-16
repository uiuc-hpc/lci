#ifndef PACKET_MANAGER_H_
#define PACKET_MANAGER_H_

#include "packet.h"
#include "macro.h"

typedef uintptr_t mv_pp;

void mv_pp_init(mv_pp**);
void mv_pp_destroy(mv_pp*);
void mv_pp_ext(mv_pp*, int nworker);

void mv_pp_free(mv_pp*, packet*, int pid);
packet* mv_pp_alloc_send(mv_pp*);
packet* mv_pp_alloc_recv_nb(mv_pp*);

#ifdef PP_NS
#include "packet_pool_ns.h"
#endif

#endif
