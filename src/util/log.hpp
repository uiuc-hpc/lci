// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_UTIL_LOG_HPP
#define LCI_UTIL_LOG_HPP

namespace lci
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

#define LCI_Assert(...) LCT_Assert(::lci::log_ctx, __VA_ARGS__)
#define LCI_Asserts(expr) LCT_Assert(::lci::log_ctx, expr, #expr)
#define LCI_Log(...) LCT_Log(::lci::log_ctx, __VA_ARGS__)
#define LCI_Warn(...) LCI_Log(::lci::LOG_WARN, "warn", __VA_ARGS__)

#ifdef LCI_DEBUG
#define LCI_DBG_Assert(...) LCI_Assert(__VA_ARGS__)
#define LCI_DBG_Log(...) LCI_Log(__VA_ARGS__)
#define LCI_DBG_Warn(...) LCI_Warn(__VA_ARGS__)
#else
#define LCI_DBG_Assert(...)
#define LCI_DBG_Log(...)
#define LCI_DBG_Warn(...)
#endif

}  // namespace lci

#endif  // LCI_UTIL_LOG_HPP
