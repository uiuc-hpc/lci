#include "lcii_config.h"
#include "lci.h"
#include "lcii.h"

uintptr_t get_dma_mem(void* server, void* buf, size_t s)
{
  return _real_server_reg(server, buf, s);
}

int free_dma_mem(uintptr_t mem)
{
  _real_server_dereg(mem);
  return 1;
}