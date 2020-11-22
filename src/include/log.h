#ifndef LC_LOG_H_
#define LC_LOG_H_

#include <stdarg.h>
#include "lci.h"

#define LCI_Log(log_level, ...) \
        LCI_Log_(log_level, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LCI_Assert(Expr, ...) \
        LCI_Assert_(#Expr, Expr, __FILE__, __func__, __LINE__, __VA_ARGS__)

#ifdef LCI_DEBUG
  #define LCI_DBG_Assert(...) LCI_Assert(__VA_ARGS__)
  #define LCI_DBG_Log(...) LCI_Log(__VA_ARGS__)
#else
  #define LCI_DBG_Assert(...)
  #define LCI_DBG_Log(...)
#endif

/**
 * LCI log level type.
 */
enum LCI_log_level_t {
  LCI_LOG_WARN = 0,
  LCI_LOG_TRACE,
  LCI_LOG_INFO,
  LCI_LOG_DEBUG,
  LCI_LOG_MAX
};

LCI_API
void LCI_Assert_(const char *expr_str, bool expr, const char *file,
                 const char *func, int line, const char *format, ...)
    __attribute__ ((__format__ (__printf__, 6, 7)));

LCI_API
void LCI_Log_(enum LCI_log_level_t log_level, const char *file,
              const char *func, int line, const char *format, ...)
    __attribute__ ((__format__ (__printf__, 5, 6)));

#endif // LC_LOG_H_
