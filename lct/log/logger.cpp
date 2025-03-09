#include <utility>
#include <vector>
#include <stdarg.h>
#include <unistd.h>
#include <stdexcept>
#include "lcti.hpp"

namespace lct
{
namespace log
{
struct ctx_t {
  ctx_t(const std::vector<std::string>& log_levels_, std::string ctx_name_,
        int default_log_level, char* filename, char* log_level_str,
        char* whitelist, char* blacklist)
      : log_levels(log_levels_),
        ctx_name(std::move(ctx_name_)),
        whitelist(whitelist),
        blacklist(blacklist)
  {
    // set log level
    log_level_setting = default_log_level;
    if (log_level_str) {
      bool succeed = false;
      for (int i = 0; i < log_levels_.size(); ++i) {
        if (log_levels_[i] == std::string(log_level_str)) {
          log_level_setting = i;
          succeed = true;
          break;
        }
      }
      if (!succeed) {
        fprintf(stderr, "%s: unknown log_level %s. use the default %s.\n",
                ctx_name.c_str(), log_level_str, log_levels_[0].c_str());
      }
    }
    // set output file
    if (filename == nullptr || strcmp(filename, "stderr") == 0)
      outfile = stderr;
    else if (strcmp(filename, "stdout") == 0)
      outfile = stdout;
    else {
      std::string ofilename =
          replaceOne(filename, "%", std::to_string(LCT_get_rank()));
      outfile = fopen(ofilename.c_str(), "a");
      if (outfile == nullptr) {
        fprintf(stderr, "Cannot open the logfile %s!\n", filename);
      }
    }
  }

  ~ctx_t()
  {
    if (outfile != stdout && outfile != stderr) {
      fclose(outfile);
    }
  }

  void report_false_assert(const char* expr_str, const char* file,
                           const char* func, int line, const char* format,
                           va_list vargs)
  {
    char buf[1024];
    int size;

    size =
        snprintf(buf, sizeof(buf), "%d:%d:%d:%s:%s:%d<%s:Assert failed: %s> ",
                 LCT_get_rank(), getpid(), LCT_get_thread_id(), file, func,
                 line, ctx_name.c_str(), expr_str);

    vsnprintf(buf + size, sizeof(buf) - size, format, vargs);

    throw std::runtime_error(buf);
  }

  void do_log(int log_level, const char* log_tag, const char* file,
              const char* func, int line, const char* format, va_list vargs)
  {
    char buf[2048];
    int size;
    LCT_Assert(LCT_log_ctx_default, log_level < log_levels.size(),
               "Unexpected log level!\n");
    // if log_level is weaker than the configured log level, do nothing.
    if (log_level > log_level_setting) return;
    // if whitelist is enabled and log_type is not include in the whitelist,
    // do nothing.
    if (whitelist != nullptr && strstr(whitelist, log_tag) == nullptr) return;
    // if blacklist is enabled and log_type is not include in the blacklist,
    // do nothing.
    if (blacklist != nullptr && strstr(blacklist, log_tag) != nullptr) return;
    // print the log
    size = snprintf(buf, sizeof(buf), "%d:%s:%d:%d:%s:%s:%d<%s:%s:%s> ",
                    LCT_get_rank(), LCT_hostname, getpid(), LCT_get_thread_id(),
                    file, func, line, ctx_name.c_str(),
                    log_levels[log_level].c_str(), log_tag);

    vsnprintf(buf + size, sizeof(buf) - size, format, vargs);

    fprintf(outfile, "%s", buf);
  }

  void do_flush() { fflush(outfile); }

  std::vector<std::string> log_levels;
  std::string ctx_name;
  int log_level_setting;
  char* whitelist;
  char* blacklist;
  FILE* outfile;
};

}  // namespace log
}  // namespace lct

LCT_log_ctx_t LCT_log_ctx_alloc(const char* const log_levels_[], int count,
                                int default_log_level, const char* ctx_name,
                                char* filename, char* log_level_str,
                                char* whitelist, char* blacklist)
{
  std::vector<std::string> log_levels;
  log_levels.resize(count);
  for (int i = 0; i < count; ++i) {
    log_levels[i] = log_levels_[i];
  }
  auto* ctx =
      new lct::log::ctx_t(log_levels, ctx_name, default_log_level, filename,
                          log_level_str, whitelist, blacklist);
  return ctx;
}

void LCT_log_ctx_free(LCT_log_ctx_t* log_ctx_p)
{
  if (LCT_unlikely(log_ctx_p == nullptr || *log_ctx_p == nullptr)) {
    if (LCT_log_ctx_default)
      LCT_Warn(LCT_log_ctx_default, "LCT_log_ctx_free: Invalid log context!\n");
    else
      fprintf(stderr, "LCT_log_ctx_free: LCT_log_ctx_default is invalid!\n");
    return;
  }
  auto* ctx = static_cast<lct::log::ctx_t*>(*log_ctx_p);
  delete ctx;
  *log_ctx_p = nullptr;
}

int LCT_log_get_level(LCT_log_ctx_t log_ctx)
{
  if (LCT_unlikely(log_ctx == nullptr)) {
    if (LCT_log_ctx_default)
      LCT_Warn(LCT_log_ctx_default,
               "LCT_log_get_level: Invalid log context!\n");
    else
      fprintf(stderr, "LCT_log_get_level: LCT_log_ctx_default is invalid!\n");
    return 0;
  }
  auto* ctx = static_cast<lct::log::ctx_t*>(log_ctx);
  return ctx->log_level_setting;
}

void LCT_Report_false_assert(LCT_log_ctx_t log_ctx, const char* expr_str,
                             const char* file, const char* func, int line,
                             const char* format, ...)
{
  if (LCT_unlikely(log_ctx == nullptr)) {
    if (LCT_log_ctx_default)
      LCT_Warn(LCT_log_ctx_default,
               "LCT_Report_false_assert: Invalid log context!\n");
    else
      fprintf(
          stderr,
          "LCT_Report_false_assert: LCT_log_ctx_default is invalid! %s:%s:%d\n",
          file, func, line);
    return;
  }
  auto* ctx = static_cast<lct::log::ctx_t*>(log_ctx);
  va_list vargs;
  va_start(vargs, format);
  ctx->report_false_assert(expr_str, file, func, line, format, vargs);
  va_end(vargs);
}

void LCT_Logv_(LCT_log_ctx_t log_ctx, int log_level, const char* log_tag,
               const char* file, const char* func, int line, const char* format,
               va_list vargs)
{
  if (LCT_unlikely(log_ctx == nullptr)) {
    if (LCT_log_ctx_default)
      LCT_Warn(LCT_log_ctx_default, "LCT_Logv_: Invalid log context!\n");
    else
      fprintf(stderr, "LCT_Logv_: LCT_log_ctx_default is invalid! %s:%s:%d\n",
              file, func, line);
    return;
  }
  auto* ctx = static_cast<lct::log::ctx_t*>(log_ctx);
  ctx->do_log(log_level, log_tag, file, func, line, format, vargs);
}

void LCT_Log_(LCT_log_ctx_t log_ctx, int log_level, const char* log_tag,
              const char* file, const char* func, int line, const char* format,
              ...)
{
  va_list vargs;
  va_start(vargs, format);
  LCT_Logv_(log_ctx, log_level, log_tag, file, func, line, format, vargs);
  va_end(vargs);
}

void LCT_Log_flush(LCT_log_ctx_t log_ctx)
{
  if (LCT_unlikely(log_ctx == nullptr)) {
    if (LCT_log_ctx_default)
      LCT_Warn(LCT_log_ctx_default, "LCT_Log_flush: Invalid log context!\n");
    else
      fprintf(stderr, "LCT_Log_flush: LCT_log_ctx_default is invalid!\n");
    return;
  }
  auto* ctx = static_cast<lct::log::ctx_t*>(log_ctx);
  ctx->do_flush();
}
