#ifndef PROGRESS_H_
#define PROGRESS_H_

#include "mv.h"

typedef void (*p_ctx_handler)(mvh*, mv_packet* p_ctx);
typedef void (*_0_arg)(void*, uint32_t);

MV_INLINE void mv_serve_imm(mvh* mv, uint32_t);
MV_INLINE void mv_serve_send(mvh* mv, mv_packet* p_ctx, uint32_t);
MV_INLINE void mv_serve_recv(mvh* mv, mv_packet* p_ctx, uint32_t);

#endif
