#include "log.h"

static const char * const log_levels[] = {
    [LCI_LOG_WARN] = "warn",
    [LCI_LOG_TRACE] = "trace",
    [LCI_LOG_INFO] = "info",
    [LCI_LOG_DEBUG] = "debug",
    [LCI_LOG_MAX] = NULL
};

void LCI_Assert_(const char *expr_str, bool expr, const char *file,
                 const char *func, int line, const char *format, ...) {
  char buf[1024];
  int size;
  va_list vargs;

  if (!expr) {
    size = snprintf(buf, sizeof(buf), "%d:%s:%s:%d<Assert failed: %s> ", LCI_RANK, file, func,
                    line, expr_str);

    va_start(vargs, format);
    vsnprintf(buf + size, sizeof(buf) - size, format, vargs);
    va_end(vargs);

    fprintf(stderr, "%s", buf);
    abort();
  }
}

void LCI_Log_(LCI_log_level_t log_level, const char *file,
              const char *func, int line, const char *format, ...) {
  char buf[1024];
  int size;
  va_list vargs;
  LCI_Assert(log_level != LCI_LOG_NONE, "You should not use LCI_LOG_NONE!\n");
  if (log_level <= LCI_LOG_LEVEL) {
    size = snprintf(buf, sizeof(buf), "%d:%s:%s:%d<%s> ", LCI_RANK, file, func,
                    line, log_levels[log_level]);

    va_start(vargs, format);
    vsnprintf(buf + size, sizeof(buf) - size, format, vargs);
    va_end(vargs);

    fprintf(stderr, "%s", buf);
  }
}
