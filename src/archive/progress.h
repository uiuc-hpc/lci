#ifndef PROGRESS_H_
#define PROGRESS_H_

typedef void (*p_ctx_handler)(lch*, lc_packet* p_ctx);
typedef void (*_0_arg)(void*, uint32_t);

static inline void lc_serve_imm(lch* mv, uint32_t);
static inline void lc_serve_send(lch* mv, lc_packet* p_ctx, uint8_t);
static inline void lc_serve_recv(lch* mv, lc_packet* p_ctx, uint8_t);

#endif
