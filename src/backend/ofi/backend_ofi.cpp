#include "lcixx_internal.hpp"

namespace lcixx
{
namespace
{
struct fi_info* search_for_prov(struct fi_info* ofi_info, const char* prov_name)
{
  struct fi_info* cur;

  for (cur = ofi_info; cur; cur = cur->next) {
    if (strcmp(cur->fabric_attr->prov_name, prov_name) == 0) {
      return cur;
    }
  }
  return nullptr;
}
}  // namespace

ofi_net_context_impl_t::ofi_net_context_impl_t(runtime_t runtime_,
                                               net_context_t::config_t config_)
    : net_context_impl_t(runtime_, config_)
{
  // Create hint.
  char* p = getenv("LCIXX_OFI_PROVIDER_HINT");
#ifdef LCIXX_OFI_PROVIDER_HINT_DEFAULT
  if (p == nullptr) {
    p = LCIXX_OFI_PROVIDER_HINT_DEFAULT;
  }
#endif
  char* prov_name_hint = nullptr;
  if (p != nullptr) {
    prov_name_hint = (char*)malloc(std::strlen(p) + 1);
    // we don't need to explicitly free prov_name_hint later.
    // fi_freeinfo(hints) will help us free it.
    strcpy(prov_name_hint, p);
  }
  struct fi_info* hints;
  hints = fi_allocinfo();
  hints->fabric_attr->prov_name = prov_name_hint;
  hints->ep_attr->type = FI_EP_RDM;
  //  hints->domain_attr->mr_mode = FI_MR_BASIC;
  hints->domain_attr->mr_mode = FI_MR_VIRT_ADDR | FI_MR_ALLOCATED |
                                FI_MR_PROV_KEY | FI_MR_LOCAL | FI_MR_ENDPOINT;
  hints->domain_attr->threading = FI_THREAD_SAFE;
  hints->domain_attr->control_progress = FI_PROGRESS_MANUAL;
  hints->domain_attr->data_progress = FI_PROGRESS_MANUAL;
  hints->caps = FI_RMA | FI_MSG;
  hints->mode = FI_LOCAL_MR;

  // Create ofi_info.
  struct fi_info* all_infos;
  FI_SAFECALL(
      fi_getinfo(FI_VERSION(1, 6), nullptr, nullptr, 0, hints, &all_infos));
  // According to the libfabric documentation, fi_getinfo call should
  // return the endpoints that are highest performing first.
  // But it appears cxi provider doesn't follow this rule,
  // so we have to do the search ourselves.
  struct fi_info* cxi_info = search_for_prov(all_infos, "cxi");
  if (cxi_info) {
    // Found the cxi provider.
    ofi_info = fi_dupinfo(cxi_info);
  } else {
    // Just use the first ofi_info.
    ofi_info = fi_dupinfo(all_infos);
  }
  fi_freeinfo(all_infos);
  LCIXX_Log(LOG_INFO, "ofi", "Provider name: %s\n",
            ofi_info->fabric_attr->prov_name);
  LCIXX_Log(LOG_INFO, "ofi", "MR mode hints: [%s]\n",
            fi_tostr(&(hints->domain_attr->mr_mode), FI_TYPE_MR_MODE));
  LCIXX_Log(LOG_INFO, "ofi", "MR mode provided: [%s]\n",
            fi_tostr(&(ofi_info->domain_attr->mr_mode), FI_TYPE_MR_MODE));
  LCIXX_Log(LOG_INFO, "ofi", "Thread mode: %s\n",
            fi_tostr(&(ofi_info->domain_attr->threading), FI_TYPE_THREADING));
  LCIXX_Log(
      LOG_INFO, "ofi", "Control progress mode: %s\n",
      fi_tostr(&(ofi_info->domain_attr->control_progress), FI_TYPE_PROGRESS));
  LCIXX_Log(
      LOG_INFO, "ofi", "Data progress mode: %s\n",
      fi_tostr(&(ofi_info->domain_attr->data_progress), FI_TYPE_PROGRESS));
  LCIXX_Log(LOG_INFO, "ofi", "Capacities: %s\n",
            fi_tostr(&(ofi_info->caps), FI_TYPE_CAPS));
  LCIXX_Log(LOG_INFO, "ofi", "Mode: %s\n",
            fi_tostr(&(ofi_info->mode), FI_TYPE_MODE));
  LCIXX_Log(LOG_INFO, "ofi", "Fi_info provided: %s\n",
            fi_tostr(ofi_info, FI_TYPE_INFO));
  LCIXX_Log(LOG_INFO, "ofi", "Fabric attributes: %s\n",
            fi_tostr(ofi_info->fabric_attr, FI_TYPE_FABRIC_ATTR));
  LCIXX_Log(LOG_INFO, "ofi", "Domain attributes: %s\n",
            fi_tostr(ofi_info->domain_attr, FI_TYPE_DOMAIN_ATTR));
  LCIXX_Log(LOG_INFO, "ofi", "Endpoint attributes: %s\n",
            fi_tostr(ofi_info->ep_attr, FI_TYPE_EP_ATTR));
  LCIXX_Assert(ofi_info->domain_attr->cq_data_size >= 4,
               "cq_data_size (%lu) is too small!\n",
               ofi_info->domain_attr->cq_data_size);
  LCIXX_Assert(ofi_info->domain_attr->mr_key_size <= 8,
               "mr_key_size (%lu) is too large!\n",
               ofi_info->domain_attr->mr_key_size);
  fi_freeinfo(hints);

  // Create libfabric obj.
  FI_SAFECALL(fi_fabric(ofi_info->fabric_attr, &ofi_fabric, nullptr));

  if (ofi_info->ep_attr->max_msg_size < config.max_msg_size) {
    config.max_msg_size = ofi_info->ep_attr->max_msg_size;
    LCIXX_Warn(
        "Reduce max_msg_size to %lu "
        "as required by the libfabric max_msg_size attribute\n",
        config.max_msg_size);
  }
}

ofi_net_context_impl_t::~ofi_net_context_impl_t()
{
  FI_SAFECALL(fi_close((struct fid*)&ofi_fabric->fid));
  fi_freeinfo(ofi_info);
}

net_device_t ofi_net_context_impl_t::alloc_net_device(
    net_device_t::config_t config)
{
  net_device_t ret;
  ret.p_impl = new ofi_net_device_impl_t(get_handler(), config);
  return ret;
}

std::atomic<uint64_t> ofi_net_device_impl_t::g_next_rdma_key(0);

ofi_net_device_impl_t::ofi_net_device_impl_t(net_context_t context_,
                                             net_device_t::config_t config_)
    : net_device_impl_t(context_, config_)
{
  auto p_ofi_context =
      reinterpret_cast<ofi_net_context_impl_t*>(context.p_impl);
  // Create domain.
  FI_SAFECALL(fi_domain(p_ofi_context->ofi_fabric, p_ofi_context->ofi_info,
                        &ofi_domain, nullptr));
  ofi_domain_attr = p_ofi_context->ofi_info->domain_attr;

  // Create end-point;
  p_ofi_context->ofi_info->tx_attr->size = config.max_sends;
  p_ofi_context->ofi_info->rx_attr->size = config.max_recvs;
  FI_SAFECALL(
      fi_endpoint(ofi_domain, p_ofi_context->ofi_info, &ofi_ep, nullptr));

  // Create cq.
  struct fi_cq_attr cq_attr;
  memset(&cq_attr, 0, sizeof(struct fi_cq_attr));
  cq_attr.format = FI_CQ_FORMAT_DATA;
  cq_attr.size = config.max_cqes;
  FI_SAFECALL(fi_cq_open(ofi_domain, &cq_attr, &ofi_cq, nullptr));

  // Bind my ep to cq.
  FI_SAFECALL(fi_ep_bind(ofi_ep, (fid_t)ofi_cq, FI_TRANSMIT | FI_RECV));

  struct fi_av_attr av_attr;
  memset(&av_attr, 0, sizeof(av_attr));
  av_attr.type = FI_AV_MAP;
  FI_SAFECALL(fi_av_open(ofi_domain, &av_attr, &ofi_av, nullptr));
  FI_SAFECALL(fi_ep_bind(ofi_ep, (fid_t)ofi_av, 0));
  FI_SAFECALL(fi_enable(ofi_ep));

  // Now exchange end-point address.
  // assume the size of the raw address no larger than 128 bits.
  const int EP_ADDR_LEN = 6;
  size_t addrlen = 0;
  fi_getname((fid_t)ofi_ep, nullptr, &addrlen);
  LCIXX_Log(LOG_INFO, "ofi", "addrlen = %lu\n", addrlen);
  LCIXX_Assert(addrlen <= 8 * EP_ADDR_LEN, "addrlen = %lu\n", addrlen);
  uint64_t my_addr[EP_ADDR_LEN];
  FI_SAFECALL(fi_getname((fid_t)ofi_ep, my_addr, &addrlen));

  peer_addrs.resize(runtime.get_nranks());
  char key[LCT_PMI_STRING_LIMIT + 1];
  sprintf(key, "LCI_KEY_%d_%d", net_device_id, runtime.get_rank());
  char value[LCT_PMI_STRING_LIMIT + 1];
  const char* PARSE_STRING = "%016lx-%016lx-%016lx-%016lx-%016lx-%016lx";
  sprintf(value, PARSE_STRING, my_addr[0], my_addr[1], my_addr[2], my_addr[3],
          my_addr[4], my_addr[5]);
  LCT_pmi_publish(key, value);
  LCT_pmi_barrier();

  for (int i = 0; i < runtime.get_nranks(); i++) {
    if (i != runtime.get_rank()) {
      sprintf(key, "LCI_KEY_%d_%d", net_device_id, i);
      LCT_pmi_getname(i, key, value);
      uint64_t peer_addr[EP_ADDR_LEN];

      sscanf(value, PARSE_STRING, &peer_addr[0], &peer_addr[1], &peer_addr[2],
             &peer_addr[3], &peer_addr[4], &peer_addr[5]);
      int ret =
          fi_av_insert(ofi_av, (void*)peer_addr, 1, &peer_addrs[i], 0, nullptr);
      LCIXX_Assert(ret == 1, "fi_av_insert failed! ret = %d\n", ret);
    } else {
      int ret =
          fi_av_insert(ofi_av, (void*)my_addr, 1, &peer_addrs[i], 0, nullptr);
      LCIXX_Assert(ret == 1, "fi_av_insert failed! ret = %d\n", ret);
    }
  }

  LCT_pmi_barrier();
}

ofi_net_device_impl_t::~ofi_net_device_impl_t()
{
  LCT_pmi_barrier();
  FI_SAFECALL(fi_close((struct fid*)&ofi_ep->fid));
  FI_SAFECALL(fi_close((struct fid*)&ofi_cq->fid));
  FI_SAFECALL(fi_close((struct fid*)&ofi_av->fid));
  FI_SAFECALL(fi_close((struct fid*)&ofi_domain->fid));
}

mr_t ofi_net_device_impl_t::register_memory(void* address, size_t size)
{
  uint64_t rdma_key;
  if (ofi_domain_attr->mr_mode & FI_MR_PROV_KEY) {
    rdma_key = 0;
  } else {
    rdma_key = g_next_rdma_key++;
  }
  struct fid_mr* mr;
  FI_SAFECALL(fi_mr_reg(ofi_domain, address, size,
                        FI_READ | FI_WRITE | FI_REMOTE_WRITE, 0, rdma_key, 0,
                        &mr, 0));
  if (ofi_domain_attr->mr_mode & FI_MR_ENDPOINT) {
    FI_SAFECALL(fi_mr_bind(mr, &ofi_ep->fid, 0));
    FI_SAFECALL(fi_mr_enable(mr));
  }

  mr_t ret;
  ret.p_impl = new ofi_mr_impl_t();
  static_cast<ofi_mr_impl_t*>(ret.p_impl)->ofi_mr = mr;
  return ret;
}

void ofi_net_device_impl_t::deregister_memory(mr_t mr)
{
  FI_SAFECALL(fi_close(
      (struct fid*)&static_cast<ofi_mr_impl_t*>(mr.p_impl)->ofi_mr->fid));
}
}  // namespace lcixx