#include <stdio.h>
#include <stdlib.h>
#include "lcm_log.h"

LCM_API const char * const log_levels[] = {
    [LCM_LOG_WARN] = "warn",
    [LCM_LOG_TRACE] = "trace",
    [LCM_LOG_INFO] = "info",
    [LCM_LOG_DEBUG] = "debug",
    [LCM_LOG_MAX] = NULL
};
LCM_API int LCM_LOG_LEVEL = LCM_LOG_WARN;
LCM_API char *LCM_LOG_whitelist_p = NULL;
LCM_API char *LCM_LOG_blacklist_p = NULL;
LCM_API FILE *LCM_LOG_OUTFILE = NULL;

void LCM_Init(int rank)  {
  {
    char* p = getenv("LCM_LOG_LEVEL");
    if (p == NULL)
      ;
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
      LCM_Log_default(
          LCM_LOG_WARN,
          "unknown env LCM_LOG_LEVEL (%s against none|warn|trace|info|debug|max). use the default LCM_LOG_WARN.\n",
          p);
  }
  LCM_LOG_whitelist_p = getenv("LCM_LOG_WHITELIST");
  LCM_LOG_blacklist_p = getenv("LCM_LOG_BLACKLIST");
  {
    char* p = getenv("LCM_LOG_OUTFILE");
    if (p == NULL || strcmp(p, "stderr") == 0)
      LCM_LOG_OUTFILE = stderr;
    else if (strcmp(p, "stdout") == 0)
      LCM_LOG_OUTFILE = stdout;
    else {
      const int filename_max = 256;
      char filename[filename_max];
      char *p0_old = p;
      char *p0_new = strchr(p,'%');
      char *p1 = filename;
      while (p0_new) {
        long nbytes = p0_new - p0_old;
        LCM_Assert(p1 + nbytes < filename + filename_max, "Filename is too long!\n");
        memcpy(p1, p0_old, nbytes);
        p1 += nbytes;
        nbytes = snprintf(p1, filename + filename_max - p1, "%d", rank);
        p1 += nbytes;
        p0_old = p0_new + 1;
        p0_new = strchr(p0_old,'%');
      }
      strncat(p1, p0_old, filename + filename_max - p1 - 1);
      LCM_LOG_OUTFILE = fopen(filename, "w+");
      if (LCM_LOG_OUTFILE == NULL) {
        fprintf(stderr, "Cannot open the logfile %s!\n", filename);
      }
    }
  }
}

void LCM_Fina() {
  if (fclose(LCM_LOG_OUTFILE) != 0) {
    fprintf(stderr, "The log file did not close successfully!\n");
  }
}
