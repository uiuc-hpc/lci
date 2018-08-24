#include "lc.h"

#include "lc_priv.h"
#include "lc/pool.h"

#include <assert.h>

#define MAX_EP 256

struct lci_dev* lcg_dev;
lc_ep lcg_ep_list[MAX_EP];

int lcg_size;
int lcg_rank;
int lcg_ndev;
char lcg_name[256];

int lcg_current_id = 0;
int lcg_deadlock;
int lcg_nep = 0;

__thread int lcg_core_id = -1;

lc_status lc_init(int ndev, long cap1, long cap2, lc_ep* ep)
{
  lc_pm_master_init(&lcg_size, &lcg_rank, lcg_name);
  posix_memalign((void**) &lcg_dev, 64, ndev * sizeof(struct lci_dev));
  lci_dev_init(&lcg_dev[0]);
  lci_dev_init(&lcg_dev[1]);
  lci_ep_open(&lcg_dev[0], cap1 | cap2, ep);
  return LC_OK;
}

lc_status lc_ep_dup(int dev_id, long cap1, long cap2, lc_ep iep, lc_ep* oep)
{
  lci_ep_open(&lcg_dev[dev_id], cap1 | cap2, oep);
  return LC_OK;
}

lc_status lc_ep_set_alloc(lc_ep ep, lc_alloc_fn alloc, lc_free_fn free, void* ctx)
{
  ep->alloc = alloc;
  ep->free = free;
  ep->ctx = ctx;
  return LC_OK;
}

lc_status lc_finalize()
{
  return LC_OK;
}

void lc_get_proc_num(int *rank)
{
  *rank = lcg_rank;
}

void lc_get_num_proc(int *size)
{
  *size = lcg_size;
}

int lc_progress_t(int id) // TODO: make a version with index.
{
  return lc_server_progress(lcg_dev[id].handle, EP_AR_EXPL | EP_CE_FLAG);
}

int lc_progress_q(int id) // TODO: make a version with index.
{
  return lc_server_progress(lcg_dev[id].handle, EP_AR_ALLOC | EP_CE_CQ);
}

int lc_progress(int id)
{
  return lc_server_progress(lcg_dev[id].handle, 0);
}

lc_status lc_free(lc_ep ep, void* buf)
{
  ep->free(ep->ctx, buf);
  return LC_OK;
}

void lc_pm_barrier() { PMI_Barrier(); }

#ifdef USE_DREG
uintptr_t get_dma_mem(void* server, void* buf, size_t s)
{
  return _real_server_reg(server, buf, s);
}

int free_dma_mem(uintptr_t mem)
{
  _real_server_dereg(mem);
  return 1;
}
#endif
