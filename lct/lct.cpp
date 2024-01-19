#include "lcti.hpp"
#include <atomic>
#include <unistd.h>

LCT_API LCT_log_ctx_t LCT_log_ctx_default = nullptr;
LCT_API int LCT_rank = -1;
LCT_API char LCT_hostname[HOST_NAME_MAX + 1] = "uninitialized";

namespace lct
{
std::atomic<int> init_count(0);

void init()
{
  if (lct::init_count.fetch_add(1) > 0)
    // init has been called
    return;

  // initialize hostname
  memset(LCT_hostname, 0, HOST_NAME_MAX + 1);
  gethostname(LCT_hostname, HOST_NAME_MAX);

  // initialize LCT_log_ctx_default
  const char* const log_levels[] = {
      "error", "warn", "diag", "info", "debug", "trace",
  };
  LCT_log_ctx_default = LCT_log_ctx_alloc(
      log_levels, sizeof(log_levels) / sizeof(log_levels[0]), LCT_LOG_WARN,
      "lct", getenv("LCT_LOG_OUTFILE"), getenv("LCT_LOG_LEVEL"),
      getenv("LCT_LOG_WHITELIST"), getenv("LCT_LOG_BLACKLIST"));

  // check cache size
  long cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
  LCT_Assert(LCT_log_ctx_default, LCT_CACHE_LINE == cache_line_size,
             "LCT_CACHE_LINE is set incorrectly! (%ld != %d)\n", LCT_CACHE_LINE,
             cache_line_size);
}

void fina()
{
  int count = lct::init_count.fetch_sub(1);
  if (count > 1)
    // fina has not been called enough times
    return;
  else if (count <= 0) {
    fprintf(stderr, "lct::fina has been called too many times (count: %d).\n",
            count);
    return;
  }
  LCT_log_ctx_free(&LCT_log_ctx_default);
}
}  // namespace lct

void LCT_init() { lct::init(); }

void LCT_fina() { lct::fina(); }

void LCT_set_rank(int rank) { LCT_rank = rank; }

int LCT_get_rank() { return LCT_rank; }