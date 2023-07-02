#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "logger.h"

LCT_log_ctx_t LCII_log_ctx;

void LCII_log_init()
{
  const char* const log_levels[] = {
      [LCI_LOG_ERROR] = "error", [LCI_LOG_WARN] = "warn",
      [LCI_LOG_DIAG] = "diag",   [LCI_LOG_INFO] = "info",
      [LCI_LOG_DEBUG] = "debug", [LCI_LOG_TRACE] = "trace",
      [LCI_LOG_MAX] = NULL};
  LCII_log_ctx = LCT_log_ctx_alloc(
      log_levels, LCI_LOG_MAX, "lci", getenv("LCI_LOG_OUTFILE"),
      getenv("LCI_LOG_LEVEL"), getenv("LCI_LOG_WHITELIST"),
      getenv("LCI_LOG_BLACKLIST"));
}

void LCII_log_fina() { LCT_log_ctx_free(&LCII_log_ctx); }
