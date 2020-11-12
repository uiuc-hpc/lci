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

void LCI_Log_(enum LCI_log_level_t log_level, const char *str, const char *file, int line) {
  if (log_level <= LCI_LOG_LEVEL) {
    if (log_level == LCI_LOG_WARN) {
      fprintf(stderr, "WARNING!\t %s\nSource:\t\t%s, line %d\n", str, file, line);
    } else {
      printf("%s\n", str);
    }
  }
}
