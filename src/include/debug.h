#ifndef LC_DEBUG_H_
#define LC_DEBUG_H_

#include "lci.h"

#ifdef LC_DEBUG
#define dprintf printf
#else
#define dprintf(...)
#endif

#ifdef LCI_DEBUG
  #define LCI_Assert(Expr) \
    LCI_Assert_(#Expr, Expr, __FILE__, __LINE__)
  #define LCI_Log(log_level, log_str) \
        LCI_Log_(log_level, log_str, __FILE__, __LINE__)
#else
  #define LCI_Assert(...) \
    do {} while (0)
  #define LCI_Log(...) \
      do {} while (0)
#endif

LCI_API
void LCI_Assert_(const char *expr_str, bool expr, const char *file, int line);

LCI_API
void LCI_Log_(enum LCI_log_level_t log_level, const char *str, const char *file, int line);

#endif
