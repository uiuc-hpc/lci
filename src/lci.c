#include "lci.h"
#include "src/include/lci_priv.h"

lc_server* lcg_dev[8];
LCI_Endpoint lcg_endpoint[8];
int lcg_num_devices = 0;
int lcg_num_endpoints= 0;
int lcg_rank = 0;
int lcg_size = 0;
char lcg_name[64];

int lcg_current_id = 0;
int lcg_deadlock = 0;
volatile uint32_t next_key = 1;
__thread int lcg_core_id = -1;

LCI_Status LCI_Initialize(int num_devices)
{
  // Initialize processes in this job.
  lc_pm_master_init(&lcg_size, &lcg_rank, lcg_name);

  for (int i = 0; i < num_devices; i++) {
    lc_dev_init(i, &lcg_dev[i]);
  }
  lcg_num_devices = num_devices;
  return LCI_OK;
}

LCI_Status LCI_Finalize()
{
  for (int i = 0; i < lcg_num_devices; i++) {
    lc_dev_finalize(lcg_dev[i]);
  }
  return LCI_OK;
}

LCI_Status LCI_Endpoint_create(int device, LCI_Property prop, LCI_Endpoint* ep_ptr)
{
  struct LCI_Endpoint_s* ep = 0;
  lc_server* dev = lcg_dev[device];
  posix_memalign((void**) &ep, 64, sizeof(struct LCI_Endpoint_s));
  ep->server = dev;
  ep->pkpool = dev->pkpool;
  ep->gid = lcg_num_endpoints++;
  lcg_endpoint[ep->gid] = ep;

  if (prop->ctype == LCI_CT_2SIDED || prop->ctype == LCI_CT_COLL) {
    lc_hash_create(&ep->tbl);
    ep->property = EP_AR_EXP;
  } else {
    ep->property = EP_AR_DYN;
  }

  if (prop->stype == LCI_ST_SYNC) {
    ep->property |= EP_CE_SYNC;
  } else if (prop->stype == LCI_ST_AM) {
    ep->property |= EP_CE_AM;
    ep->handler = prop->s_ctx;
  } else if (prop->stype == LCI_ST_CQ) {
    ep->property |= EP_CE_CQ;
    lc_cq_create(&ep->cq);
  }

  ep->rep = dev->rep;
  *ep_ptr = ep;
  return LCI_OK;
}

LCI_Status LCI_Property_create(LCI_Property* prop_ptr)
{
  struct LCI_Property_s* prop = 0;
  posix_memalign((void**) &prop, 64, sizeof(struct LCI_Property_s));
  prop->ctype = LCI_CT_COLL;
  prop->mtype = LCI_MT_LONG;
  prop->stype = LCI_ST_SYNC;

  *prop_ptr = prop;
  return LCI_OK;
}

LCI_Status LCI_Property_set_comm_type(LCI_Comm_type type, LCI_Property* prop)
{
  (*prop)->ctype = type;
  return LCI_OK;
}

LCI_Status LCI_Property_set_message_type(LCI_Message_type type, LCI_Property* prop)
{
  (*prop)->mtype = type;
  return LCI_OK;
}

LCI_Status LCI_Property_set_sync_type(LCI_Sync_type type, void* ctx, LCI_Property* prop)
{
  (*prop)->stype = type;
  (*prop)->s_ctx = ctx;
  return LCI_OK;
}

int LCI_Rank()
{
  return lcg_rank;
}

int LCI_Size()
{
  return lcg_size;
}

LCI_Status LCI_Sync_create(LCI_Sync* sync_ptr) {
  struct LCI_Sync_s* sync = 0;
  posix_memalign((void**) &sync, 64, sizeof(struct LCI_Sync_s));
  sync->flag = 1;
  *sync_ptr = sync;
  return LCI_OK;
}

LCI_Status LCI_Sync_reset(LCI_Sync* sync_ptr) {
  struct LCI_Sync_s* sync = *sync_ptr;
  sync->flag = 1;
  return LCI_OK;
}

LCI_Status LCI_Sync_wait(LCI_Sync sync) {
  while (sync->flag)
    ;
  return LCI_OK;
}

int LCI_Sync_test(LCI_Sync sync) {
  return (sync->flag == 0);
}

LCI_Status LCI_Sync_signal(LCI_Sync sync) {
  sync->flag = 0;
  return LCI_OK;
}

LCI_Status LCI_Progress(int id, int count)
{
  lc_server_progress(lcg_dev[id]);
  return LCI_OK;
}
