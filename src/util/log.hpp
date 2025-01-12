#ifndef LCIXX_UTIL_LOG_HPP
#define LCIXX_UTIL_LOG_HPP

namespace lcixx
{
enum log_level_t {
  LOG_ERROR,
  LOG_WARN,
  LOG_STATUS,
  LOG_INFO,
  LOG_DEBUG,
  LOG_TRACE
};

extern LCT_log_ctx_t log_ctx;

void log_initialize();
void log_finalize();
static inline void log_flush() { LCT_Log_flush(log_ctx); }

#define LCIXX_Assert(...) LCT_Assert(::lcixx::log_ctx, __VA_ARGS__)
#define LCIXX_Log(...) LCT_Log(::lcixx::log_ctx, __VA_ARGS__)
#define LCIXX_Warn(...) LCIXX_Log(::lcixx::LOG_WARN, "warn", __VA_ARGS__)

#ifdef LCIXX_DEBUG
#define LCIXX_DBG_Assert(...) LCIXX_Assert(__VA_ARGS__)
#define LCIXX_DBG_Log(...) LCIXX_Log(__VA_ARGS__)
#define LCIXX_DBG_Warn(...) LCIXX_Warn(__VA_ARGS__)
#else
#define LCIXX_DBG_Assert(...)
#define LCIXX_DBG_Log(...)
#define LCIXX_DBG_Warn(...)
#endif

}  // namespace lcixx

#endif  // LCIXX_UTIL_LOG_HPP
