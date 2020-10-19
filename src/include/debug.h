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

void LCI_Assert_(const char *expr_str, bool expr, const char *file, int line) {
  if (!expr) {
    printf("Assert failed!\nExpected:\t %s\nSource:\t\t%s, line %d\n", expr_str, file, line);
    abort();
  }
}

void LCI_Warn_(const char *str, const char *file, int line) {
  printf("WARNING!\t %s\nSource:\t\t%s, line %d\n", str, file, line);
}

#endif
