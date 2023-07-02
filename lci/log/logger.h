#ifndef LCII_LOG_H_
#define LCII_LOG_H_

#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "lct.h"

enum LCI_log_level_t {
  LCI_LOG_ERROR,
  LCI_LOG_WARN,
  LCI_LOG_DIAG,
  LCI_LOG_INFO,
  LCI_LOG_DEBUG,
  LCI_LOG_TRACE,
  LCI_LOG_MAX
};

extern LCT_log_ctx_t LCII_log_ctx;

void LCII_log_init();
void LCII_log_fina();
static inline void LCI_Log_flush() { LCT_Log_flush(LCII_log_ctx); }

#define LCI_Assert(...) LCT_Assert(LCII_log_ctx, __VA_ARGS__)
#define LCI_Log(...) LCT_Log(LCII_log_ctx, __VA_ARGS__)
#define LCI_Warn(...) LCI_Log(LCI_LOG_WARN, "warn", __VA_ARGS__)

#ifdef LCI_DEBUG
#define LCI_DBG_Assert(...) LCI_Assert(__VA_ARGS__)
#define LCI_DBG_Log(...) LCI_Log(__VA_ARGS__)
#define LCI_DBG_Warn(...) LCI_Warn(__VA_ARGS__)
#else
#define LCI_DBG_Assert(...)
#define LCI_DBG_Log(...)
#define LCI_DBG_Warn(...)
#endif

#endif  // LCII_LOG_H_
