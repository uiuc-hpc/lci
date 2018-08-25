#ifndef SERVER_H_
#define SERVER_H_

LC_INLINE
void lci_serve_recv(lc_packet* p, lc_proto proto, const long cap);

LC_INLINE
void lci_serve_imm(lc_packet* p, const long cap);

LC_INLINE
void lci_serve_recv_rdma(lc_packet*, lc_proto proto);

LC_INLINE
void lci_serve_send(lc_packet* p);

#ifdef LC_USE_SERVER_OFI
#include "server_ofi.h"
#endif

#ifdef LC_USE_SERVER_IBV
#include "server_ibv.h"
#endif

#ifdef LC_USE_SERVER_PSM
#include "server_psm2.h"
#endif

#endif
