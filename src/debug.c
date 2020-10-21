//
// Created by jiakun on 2020/10/21.
//
#include "debug.h"

void LCI_Assert_(const char *expr_str, bool expr, const char *file, int line) {
  if (!expr) {
    fprintf(stderr, "Assert failed!\nExpected:\t %s\nSource:\t\t%s, line %d\n", expr_str, file, line);
    abort();
  }
}

void LCI_Warn_(const char *str, const char *file, int line) {
  fprintf(stderr, "WARNING!\t %s\nSource:\t\t%s, line %d\n", str, file, line);
}