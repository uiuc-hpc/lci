// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
LCT_log_ctx_t log_ctx;

void log_initialize()
{
  const char* const log_levels[] = {"error", "warn",  "status",
                                    "info",  "debug", "trace"};
  log_ctx = LCT_log_ctx_alloc(
      log_levels, sizeof(log_levels) / sizeof(log_levels[0]), LOG_WARN, "lci",
      getenv("LCI_LOG_OUTFILE"), getenv("LCI_LOG_LEVEL"),
      getenv("LCI_LOG_WHITELIST"), getenv("LCI_LOG_BLACKLIST"));
}

void log_finalize() { LCT_log_ctx_free(&log_ctx); }

}  // namespace lci