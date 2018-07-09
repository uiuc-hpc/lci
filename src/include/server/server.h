#ifndef SERVER_H_
#define SERVER_H_

LC_INLINE
void lc_serve_recv(lc_hw hw, lc_packet* p, uint32_t proto, const long cap);

LC_INLINE
void lc_serve_send(lc_hw hw, lc_packet* p, uint32_t proto);

LC_INLINE
void lc_serve_imm(struct lci_ep* ep, uint32_t imm);

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
