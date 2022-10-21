#ifndef LCM_LOG_H_
#define LCM_LOG_H_

#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define LCM_API __attribute__((visibility("default")))

#define LCM_Assert(Expr, ...) \
        LCM_Assert_(#Expr, Expr, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LCM_Log(log_level, log_type, ...) \
        LCM_Log_(log_level, log_type, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LCM_Log_default(log_level, ...) \
        LCM_Log(log_level, "default", __VA_ARGS__)

#ifdef LCM_DEBUG
#define LCM_DBG_Assert(...) LCM_Assert(__VA_ARGS__)
#define LCM_DBG_Log(...) LCM_Log(__VA_ARGS__)
#define LCM_DBG_Log_default(...) LCM_Log_default(__VA_ARGS__)
#else
#define LCM_DBG_Assert(...)
#define LCM_DBG_Log(...)
#define LCM_DBG_Log_default(...)
#endif

enum LCM_log_level_t {
  LCM_LOG_NONE = 0,
  LCM_LOG_WARN,
  LCM_LOG_TRACE,
  LCM_LOG_INFO,
  LCM_LOG_DEBUG,
  LCM_LOG_MAX
};

extern const char * const log_levels[LCM_LOG_MAX + 1];

void LCM_Init(int rank);

void LCM_Fina();

static inline void LCM_Assert_(const char *expr_str, int expr, const char *file,
                  const char *func, int line, const char *format, ...)
__attribute__((__format__(__printf__, 6, 7)));

static inline void LCM_Log_(enum LCM_log_level_t log_level, const char *log_type,
              const char *file, const char *func, int line,
              const char *format, ...)
__attribute__((__format__(__printf__, 6, 7)));

static inline void LCM_Log_flush();

/* =============== Implementation ================*/

extern int LCM_LOG_LEVEL;
extern char *LCM_LOG_whitelist_p;
extern char *LCM_LOG_blacklist_p;
extern FILE *LCM_LOG_OUTFILE;

void LCM_Assert_(const char *expr_str, int expr, const char *file,
                 const char *func, int line, const char *format, ...) {
  if (expr)
    return;

  char buf[1024];
  int size;
  va_list vargs;

  size = snprintf(buf, sizeof(buf), "%d:%s:%s:%d<Assert failed: %s> ", getpid(), file, func,
                  line, expr_str);

  va_start(vargs, format);
  vsnprintf(buf + size, sizeof(buf) - size, format, vargs);
  va_end(vargs);

  fprintf(stderr, "%s", buf);
  abort();
}

void LCM_Log_(enum LCM_log_level_t log_level, const char *log_type,
              const char *file, const char *func, int line,
              const char *format, ...) {
  char buf[1024];
  int size;
  va_list vargs;
  LCM_Assert(log_level != LCM_LOG_NONE, "You should not use LCM_LOG_NONE!\n");
  // if log_level is weaker than the configured log level, do nothing.
  if (log_level > LCM_LOG_LEVEL)
    return;
  // if whitelist is enabled and log_type is not include in the whitelist,
  // do nothing.
  if (LCM_LOG_whitelist_p != NULL &&
      strstr(LCM_LOG_whitelist_p, log_type) == NULL)
    return;
  // if blacklist is enabled and log_type is not include in the blacklist,
  // do nothing.
  if (LCM_LOG_blacklist_p != NULL &&
      strstr(LCM_LOG_blacklist_p, log_type) != NULL)
    return;
  // print the log
  size = snprintf(buf, sizeof(buf), "%d:%s:%s:%d<%s:%s> ",
                  getpid(), file, func, line, log_levels[log_level], log_type);

  va_start(vargs, format);
  vsnprintf(buf + size, sizeof(buf) - size, format, vargs);
  va_end(vargs);

  fprintf(LCM_LOG_OUTFILE, "%s", buf);
}

static inline void LCM_Log_flush() {
  fflush(LCM_LOG_OUTFILE);
}

#if defined(__cplusplus)
}
#endif

#endif // LCM_LOG_H_
