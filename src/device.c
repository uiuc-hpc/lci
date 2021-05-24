#include "lcii.h"

lc_server** LCI_DEVICES;

void lc_dev_init(int id, lc_server** dev, LCI_plist_t *plist)
{
  uintptr_t base_packet;
  lc_server_init(id, dev);
  lc_server* s = *dev;
  LCI_plist_create(plist);

  LCII_mt_init(&s->mt, 0);
  uintptr_t base_addr = s->heap_addr;
  base_packet = base_addr + 8192 - sizeof(struct packet_context);

  lc_pool_create(&s->pkpool);
  for (int i = 0; i < LC_SERVER_NUM_PKTS; i++) {
    lc_packet* p = (lc_packet*)(base_packet + i * LC_PACKET_SIZE);
    p->context.pkpool = s->pkpool;
    p->context.poolid = 0;
    lc_pool_put(s->pkpool, p);
  }
}

void lc_dev_finalize(lc_server* dev)
{
  int total_num = lc_pool_count(dev->pkpool) + dev->recv_posted;
  if (total_num != LC_SERVER_NUM_PKTS)
    LCM_DBG_Log(LCM_LOG_WARN, "Potentially losing packets %d != %d\n", total_num, LC_SERVER_NUM_PKTS);
  LCII_mt_free(&dev->mt);
  lc_pool_destroy(dev->pkpool);
  lc_server_finalize(dev);
}