#include "lcixx_internal.hpp"

namespace lcixx
{
LCT_log_ctx_t log_ctx;

void log_initialize()
{
  const char* const log_levels[] = {
      [LOG_ERROR] = "error", [LOG_WARN] = "warn",   [LOG_STATUS] = "status",
      [LOG_INFO] = "info",   [LOG_DEBUG] = "debug", [LOG_TRACE] = "trace"};
  log_ctx = LCT_log_ctx_alloc(
      log_levels, sizeof(log_levels) / sizeof(log_levels[0]), LOG_WARN, "lci",
      getenv("LCIXX_LOG_OUTFILE"), getenv("LCIXX_LOG_LEVEL"),
      getenv("LCIXX_LOG_WHITELIST"), getenv("LCIXX_LOG_BLACKLIST"));
}

void log_finalize() { LCT_log_ctx_free(&log_ctx); }

}  // namespace lcixx