#include "lci.h"
#include "src/include/lci_priv.h"
#include "src/include/config.h"

void lc_config_init(int num_proc, int rank)
{
  LCI_IMMEDIATE_LENGTH = LC_MAX_INLINE;
  LCI_BUFFERED_LENGTH = LC_PACKET_SIZE;
  LCI_NUM_DEVICES = 1;
  LCI_NUM_ENDPOINTS = 8;
  LCI_NUM_PROCESSES = num_proc;
  LCI_RANK = rank;
  LCI_REGISTERED_MEMORY_SIZE = LC_DEV_MEM_SIZE;
  char* val = getenv("LCI_REGISTER_MEMORY_SIZE");
  if (val != NULL) {
    LCI_REGISTERED_MEMORY_SIZE = atoi(val);
  }

  LCI_DEVICES = calloc(sizeof(lc_server*), LCI_NUM_DEVICES);
  LCI_ENDPOINTS = calloc(sizeof(LCI_endpoint_t), LCI_NUM_ENDPOINTS);
}
