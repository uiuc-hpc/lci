#include "lcii_config.h"
#include "lci.h"
#include "lcii.h"

uintptr_t get_dma_mem(void* server, void* buf, size_t s)
{
  return (uintptr_t)LCISI_real_server_reg(server, buf, s);
}

int free_dma_mem(uintptr_t mem)
{
  LCISI_real_server_dereg((void*)mem);
  return 1;
}