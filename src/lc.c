#include "lc.h"
#include "lc_priv.h"
#include "lc/pool.h"

#include <assert.h>
#include <unistd.h>

lc_server* lcg_dev[LC_MAX_DEV];
struct lci_ep* lcg_ep[LC_MAX_EP];

int lcg_size;
int lcg_rank;
int lcg_ndev;
char lcg_name[256];

int lcg_current_id = 0;
int lcg_deadlock = 0;
int lcg_nep = 0;
int lcg_page_size = 0;

__thread int lcg_core_id = -1;

lc_status lc_init(int ndev, lc_ep* ep)
{
  lcg_page_size = sysconf(_SC_PAGESIZE);
  lc_pm_master_init(&lcg_size, &lcg_rank, lcg_name);

  for (int i = 0; i < ndev; i++) {
    lci_dev_init(i, &lcg_dev[i]);
  }
  lci_ep_open(lcg_dev[0], LC_EXP_SYNC.addr | LC_EXP_SYNC.ce, ep);
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

int lc_progress(int id)
{
  return lc_server_progress(lcg_dev[id]);
}
