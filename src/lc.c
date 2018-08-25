#include "lc.h"
#include "lc_priv.h"
#include "lc/pool.h"

#include <assert.h>
#include <unistd.h>

#define MAX_EP 256

struct lci_dev* lcg_dev;
struct lci_ep**  lcg_ep_list;

int lcg_size;
int lcg_rank;
int lcg_ndev;
char lcg_name[256];

int lcg_current_id = 0;
int lcg_deadlock = 0;
int lcg_nep = 0;
int lcg_page_size = 0;

__thread int lcg_core_id = -1;

lc_status lc_init(int ndev, lc_ep_desc desc, lc_ep* ep)
{
  lcg_page_size = sysconf(_SC_PAGESIZE);

  lc_pm_master_init(&lcg_size, &lcg_rank, lcg_name);
  posix_memalign((void**) &lcg_dev, LC_CACHE_LINE, ndev * sizeof(struct lci_dev));
  posix_memalign((void**) &lcg_ep_list, LC_CACHE_LINE, MAX_EP * sizeof(struct lci_ep*));
  for (int i = 0; i < ndev; i++) {
    lci_dev_init(&lcg_dev[i]);
  }
  lci_ep_open(&lcg_dev[0], desc.addr | desc.ce, ep);
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

int lc_progress_t(int id)
{
  return lc_server_progress(lcg_dev[id].handle, EP_AR_EXPL | EP_CE_SYNC);
}

int lc_progress_q(int id)
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
