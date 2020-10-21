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
  #define LCI_WARNING(Str) \
    LCI_Warn_(Str, __FILE__, __LINE__)
#else
  #define LCI_Assert(Expr) ;
  #define LCI_WARNING(Str) ;
#endif

LCI_API
void LCI_Assert_(const char *expr_str, bool expr, const char *file, int line);

LCI_API
void LCI_Warn_(const char *str, const char *file, int line);

#endif
