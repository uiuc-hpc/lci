#include <limits.h>
#include "lci.h"
#include "lci_priv.h"
#include "config.h"

int LCI_NUM_DEVICES;
int LCI_NUM_PROCESSES;
int LCI_RANK;
int LCI_MAX_ENDPOINTS;
int LCI_MAX_TAG = (1u << 15) - 1;
int LCI_IMMEDIATE_SIZE = 8;
int LCI_BUFFERED_SIZE = LC_PACKET_SIZE - sizeof(struct packet_context);
int LCI_REGISTERED_SEGMENT_SIZE;
int LCI_REGISTERED_SEGMENT_START = -1;
int LCI_MAX_REGISTERED_SEGMENT_SIZE = INT_MAX;
int LCI_MAX_REGISTERED_SEGMENT_NUMBER = 1;
int LCI_DEFAULT_MT_LENGTH = 1u << TBL_BIT_SIZE;
int LCI_MAX_MT_LENGTH = 1u << TBL_BIT_SIZE;
int LCI_DEFAULT_CQ_LENGTH = CQ_MAX_SIZE;
int LCI_MAX_CQ_LENGTH = CQ_MAX_SIZE;
int LCI_LOG_LEVEL = LCI_LOG_WARN;
int LCI_PMI_VERSION = LCI_USE_PMI;

static inline int getenv_or(char* env, int def) {
  char* val = getenv(env);
  if (val != NULL) {
    return atoi(val);
  } else {
    return def;
  }
}

void lc_config_init(int num_proc, int rank)
{
  char *p;

  LCI_NUM_DEVICES = getenv_or("LCI_NUM_DEVICES", 1);
  LCI_MAX_ENDPOINTS = getenv_or("LCI_MAX_ENDPOINTS", 8);
  LCI_NUM_PROCESSES = num_proc;
  LCI_RANK = rank;
  LCI_REGISTERED_SEGMENT_SIZE = getenv_or("LCI_REGISTERED_SEGMENT_SIZE", LC_DEV_MEM_SIZE);

  p = getenv("LCI_LOG_LEVEL");
  if (p == NULL) ;
  else if (strcmp(p, "none") == 0 || strcmp(p, "NONE") == 0)
    LCI_LOG_LEVEL = LCI_LOG_NONE;
  else if (strcmp(p, "warn") == 0 || strcmp(p, "WARN") == 0)
    LCI_LOG_LEVEL = LCI_LOG_WARN;
  else if (strcmp(p, "trace") == 0 || strcmp(p, "TRACE") == 0)
    LCI_LOG_LEVEL = LCI_LOG_TRACE;
  else if (strcmp(p, "info") == 0 || strcmp(p, "INFO") == 0)
    LCI_LOG_LEVEL = LCI_LOG_INFO;
  else if (strcmp(p, "debug") == 0 || strcmp(p, "DEBUG") == 0)
    LCI_LOG_LEVEL = LCI_LOG_DEBUG;
  else if (strcmp(p, "max") == 0 || strcmp(p, "MAX") == 0)
    LCI_LOG_LEVEL = LCI_LOG_MAX;
  else
    LCI_Log(LCI_LOG_WARN, "unknown env LCI_LOG_LEVEL (%s against none|warn|trace|info|debug|max). use the default LCI_LOG_WARN.\n", p);

  p = getenv("LCI_PMI_VERSION");
  if (p == NULL) ;
  else if (strcmp(p, "pmi") == 0 || strcmp(p, "PMI") == 0)
    LCI_PMI_VERSION = LCI_USE_PMI;
  else if (strcmp(p, "pmi2") == 0 || strcmp(p, "PMI2") == 0)
    LCI_PMI_VERSION = LCI_USE_PMI2;
  else
    LCI_Log(LCI_LOG_WARN, "unknown env LCI_PMI_VERSION (%s against pmi|pmi2). use the default PMIv1.\n", p);

  LCI_DEVICES = calloc(sizeof(lc_server*), LCI_NUM_DEVICES);
  LCI_ENDPOINTS = calloc(sizeof(LCI_endpoint_t), LCI_MAX_ENDPOINTS);
}
