#include "lcii.h"

static int g_device_num = 0;
int g_next_rdma_key = 0;

void lc_server_init(LCI_device_t device, LCID_server_t* s)
{
  int device_id = g_device_num++;
  LCIDI_server_t *server = LCIU_malloc(sizeof(LCIDI_server_t));
  *s = (LCID_server_t) server;
  server->device = device;

  // Create hint.
  struct fi_info* hints;
  hints = fi_allocinfo();
  hints->ep_attr->type = FI_EP_RDM;
//  hints->domain_attr->mr_mode = FI_MR_BASIC;
  hints->domain_attr->mr_mode = FI_MR_VIRT_ADDR | FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_LOCAL;
  hints->domain_attr->threading = FI_THREAD_SAFE;
  hints->domain_attr->control_progress = FI_PROGRESS_MANUAL;
  hints->domain_attr->data_progress = FI_PROGRESS_MANUAL;
  hints->caps = FI_RMA | FI_TAGGED;
  hints->mode = FI_LOCAL_MR;

  // Create info.
  FI_SAFECALL(fi_getinfo(FI_VERSION(1, 6), NULL, NULL, 0, hints, &server->fi));
  LCM_Log(LCM_LOG_INFO, "Provider name: %s\n", server->fi->fabric_attr->prov_name);
  LCM_Log(LCM_LOG_INFO, "MR mode hints: [%s]\n", fi_tostr(&(hints->domain_attr->mr_mode), FI_TYPE_MR_MODE));
  LCM_Log(LCM_LOG_INFO, "MR mode provided: [%s]\n", fi_tostr(&(server->fi->domain_attr->mr_mode), FI_TYPE_MR_MODE));
  LCM_Log(LCM_LOG_INFO, "Thread mode: %s\n", fi_tostr(&(server->fi->domain_attr->threading), FI_TYPE_THREADING));
  LCM_Log(LCM_LOG_INFO, "Control progress mode: %s\n", fi_tostr(&(server->fi->domain_attr->control_progress), FI_TYPE_PROGRESS));
  LCM_Log(LCM_LOG_INFO, "Data progress mode: %s\n", fi_tostr(&(server->fi->domain_attr->data_progress), FI_TYPE_PROGRESS));
  LCM_Log(LCM_LOG_MAX, "Fi_info provided: %s\n", fi_tostr(server->fi, FI_TYPE_INFO));
  LCM_Log(LCM_LOG_MAX, "Fabric attributes: %s\n", fi_tostr(server->fi->fabric_attr, FI_TYPE_FABRIC_ATTR));
  LCM_Log(LCM_LOG_MAX, "Domain attributes: %s\n", fi_tostr(server->fi->domain_attr, FI_TYPE_DOMAIN_ATTR));
  LCM_Log(LCM_LOG_MAX, "Endpoint attributes: %s\n", fi_tostr(server->fi->ep_attr, FI_TYPE_EP_ATTR));
  LCM_Assert(server->fi->domain_attr->cq_data_size >= 4, "cq_data_size = %lu\n", server->fi->domain_attr->cq_data_size);
  LCM_Assert(server->fi->domain_attr->mr_key_size <= 8, "mr_key_size = %lu\n", server->fi->domain_attr->mr_key_size);
  fi_freeinfo(hints);

  // Create libfabric obj.
  FI_SAFECALL(fi_fabric(server->fi->fabric_attr, &server->fabric, NULL));

  // Create domain.
  FI_SAFECALL(fi_domain(server->fabric, server->fi, &server->domain, NULL));

  // Create end-point;
  FI_SAFECALL(fi_endpoint(server->domain, server->fi, &server->ep, NULL));

  // Create cq.
  struct fi_cq_attr cq_attr;
  memset(&cq_attr, 0, sizeof(struct fi_cq_attr));
  cq_attr.format = FI_CQ_FORMAT_TAGGED;
  cq_attr.size = MAX_CQ_SIZE;
  FI_SAFECALL(fi_cq_open(server->domain, &cq_attr, &server->cq, NULL));

  // Bind my ep to cq.
  FI_SAFECALL(fi_ep_bind(server->ep, (fid_t)server->cq, FI_TRANSMIT | FI_RECV));

  struct fi_av_attr av_attr;
  memset(&av_attr, 0, sizeof(av_attr));
  av_attr.type = FI_AV_MAP;
  FI_SAFECALL(fi_av_open(server->domain, &av_attr, &server->av, NULL));
  FI_SAFECALL(fi_ep_bind(server->ep, (fid_t)server->av, 0));
  FI_SAFECALL(fi_enable(server->ep));

  // Now exchange end-point address.
  // assume the size of the raw address no larger than 128 bits.
  const int EP_ADDR_LEN = 6;
  size_t addrlen = 0;
  fi_getname((fid_t)server->ep, NULL, &addrlen);
  LCM_Log(LCM_LOG_INFO, "addrlen = %lu\n", addrlen);
  LCM_Assert(addrlen <= 8 * EP_ADDR_LEN, "addrlen = %lu\n", addrlen);
  uint64_t my_addr[EP_ADDR_LEN];
  FI_SAFECALL(fi_getname((fid_t)server->ep, my_addr, &addrlen));

  server->peer_addrs = LCIU_malloc(sizeof(fi_addr_t) * LCI_NUM_PROCESSES);
  char msg[256];
  const char* PARSE_STRING = "%016lx-%016lx-%016lx-%016lx-%016lx-%016lx";
  sprintf(msg, PARSE_STRING,
          my_addr[0], my_addr[1], my_addr[2], my_addr[3], my_addr[4], my_addr[5]);
  lc_pm_publish(LCI_RANK, device_id, msg);
  lc_pm_barrier();

  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    if (i != LCI_RANK) {
      lc_pm_getname(i, device_id, msg);
      uint64_t peer_addr[EP_ADDR_LEN];

      sscanf(msg, PARSE_STRING,
             &peer_addr[0], &peer_addr[1], &peer_addr[2], &peer_addr[3], &peer_addr[4], &peer_addr[5]);
      int ret = fi_av_insert(server->av, (void*)peer_addr, 1, &server->peer_addrs[i], 0, NULL);
      LCM_Assert(ret == 1, "fi_av_insert failed! ret = %d\n", ret);
    } else {
      int ret = fi_av_insert(server->av, (void*)my_addr, 1, &server->peer_addrs[i], 0, NULL);
      LCM_Assert(ret == 1, "fi_av_insert failed! ret = %d\n", ret);
    }
  }

  server->recv_posted = 0;
}

void lc_server_finalize(LCID_server_t s)
{
  LCIDI_server_t *server = (LCIDI_server_t*) s;
  LCIU_free(server->peer_addrs);
  FI_SAFECALL(fi_close((struct fid*) &server->ep->fid));
  FI_SAFECALL(fi_close((struct fid*) &server->cq->fid));
  FI_SAFECALL(fi_close((struct fid*) &server->av->fid));
  FI_SAFECALL(fi_close((struct fid*) &server->domain->fid));
  free(s);
}