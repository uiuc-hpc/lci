#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

static const char * const log_levels[] = {
    [LCM_LOG_WARN] = "warn",
    [LCM_LOG_TRACE] = "trace",
    [LCM_LOG_INFO] = "info",
    [LCM_LOG_DEBUG] = "debug",
    [LCM_LOG_MAX] = NULL
};

static int LCM_LOG_LEVEL = LCM_LOG_WARN;

int LCM_Init()  {
  char *p = getenv("LCM_LOG_LEVEL");
  if (p == NULL) ;
  else if (strcmp(p, "none") == 0 || strcmp(p, "NONE") == 0)
    LCM_LOG_LEVEL = LCM_LOG_NONE;
  else if (strcmp(p, "warn") == 0 || strcmp(p, "WARN") == 0)
    LCM_LOG_LEVEL = LCM_LOG_WARN;
  else if (strcmp(p, "trace") == 0 || strcmp(p, "TRACE") == 0)
    LCM_LOG_LEVEL = LCM_LOG_TRACE;
  else if (strcmp(p, "info") == 0 || strcmp(p, "INFO") == 0)
    LCM_LOG_LEVEL = LCM_LOG_INFO;
  else if (strcmp(p, "debug") == 0 || strcmp(p, "DEBUG") == 0)
    LCM_LOG_LEVEL = LCM_LOG_DEBUG;
  else if (strcmp(p, "max") == 0 || strcmp(p, "MAX") == 0)
    LCM_LOG_LEVEL = LCM_LOG_MAX;
  else
    LCM_Log(LCM_LOG_WARN, "unknown env LCM_LOG_LEVEL (%s against none|warn|trace|info|debug|max). use the default LCM_LOG_WARN.\n", p);
  return LCM_LOG_LEVEL;
}

void LCM_Assert_(const char *expr_str, int expr, const char *file,
                  const char *func, int line, const char *format, ...) {
  char buf[1024];
  int size;
  va_list vargs;

  if (!expr) {
    size = snprintf(buf, sizeof(buf), "%d:%s:%s:%d<Assert failed: %s> ", getpid(), file, func,
                    line, expr_str);

    va_start(vargs, format);
    vsnprintf(buf + size, sizeof(buf) - size, format, vargs);
    va_end(vargs);

    fprintf(stderr, "%s", buf);
    abort();
  }
}

void LCM_Log_(enum LCM_log_level_t log_level, const char *file,
               const char *func, int line, const char *format, ...) {
  char buf[1024];
  int size;
  va_list vargs;
  LCM_Assert(log_level != LCM_LOG_NONE, "You should not use LCM_LOG_NONE!\n");
  if (log_level <= LCM_LOG_LEVEL) {
    size = snprintf(buf, sizeof(buf), "%d:%s:%s:%d<%s> ", getpid(), file, func,
                    line, log_levels[log_level]);

    va_start(vargs, format);
    vsnprintf(buf + size, sizeof(buf) - size, format, vargs);
    va_end(vargs);

    fprintf(stderr, "%s", buf);
  }
}
