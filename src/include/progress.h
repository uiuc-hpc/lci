#ifndef PROGRESS_H_
#define PROGRESS_H_

#include "mv.h"
#include "request.h"

extern int PROTO_SHORT;
extern int PROTO_RECV_READY;
extern int PROTO_READY_FIN;
extern int PROTO_AM;
extern int PROTO_SEND_WRITE_FIN;

typedef void (*p_ctx_handler)(mvh*, mv_packet* p_ctx);
typedef void (*_0_arg)(void*, uint32_t);

void mv_progress_init(mvh* mv);
void mv_serve_imm(uint32_t);
void mv_serve_send(mvh* mv, mv_packet* p_ctx);
void mv_serve_recv(mvh* mv, mv_packet* p_ctx);

#endif
