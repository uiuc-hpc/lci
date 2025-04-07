// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
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

ofi_net_context_impl_t::ofi_net_context_impl_t(runtime_t runtime_, attr_t attr_)
    : net_context_impl_t(runtime_, attr_)
{
  const char* p = attr.ofi_provider_name.c_str();
  char* prov_name_hint = nullptr;
  if (p != nullptr && std::strlen(p) > 0) {
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
  hints->domain_attr->threading = FI_THREAD_SAFE;
  hints->tx_attr->inject_size = attr.max_inject_size;
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
  LCI_Log(LOG_INFO, "ofi", "Provider name: %s\n",
          ofi_info->fabric_attr->prov_name);
  LCI_Log(LOG_INFO, "ofi", "MR mode hints: [%s]\n",
          fi_tostr(&(hints->domain_attr->mr_mode), FI_TYPE_MR_MODE));
  LCI_Log(LOG_INFO, "ofi", "MR mode provided: [%s]\n",
          fi_tostr(&(ofi_info->domain_attr->mr_mode), FI_TYPE_MR_MODE));
  LCI_Log(LOG_INFO, "ofi", "Thread mode: %s\n",
          fi_tostr(&(ofi_info->domain_attr->threading), FI_TYPE_THREADING));
  LCI_Log(
      LOG_INFO, "ofi", "Control progress mode: %s\n",
      fi_tostr(&(ofi_info->domain_attr->control_progress), FI_TYPE_PROGRESS));
  LCI_Log(LOG_INFO, "ofi", "Data progress mode: %s\n",
          fi_tostr(&(ofi_info->domain_attr->data_progress), FI_TYPE_PROGRESS));
  LCI_Log(LOG_INFO, "ofi", "Capacities: %s\n",
          fi_tostr(&(ofi_info->caps), FI_TYPE_CAPS));
  LCI_Log(LOG_INFO, "ofi", "Mode: %s\n",
          fi_tostr(&(ofi_info->mode), FI_TYPE_MODE));
  LCI_Log(LOG_INFO, "ofi", "Fi_info provided: %s\n",
          fi_tostr(ofi_info, FI_TYPE_INFO));
  LCI_Log(LOG_INFO, "ofi", "Fabric attributes: %s\n",
          fi_tostr(ofi_info->fabric_attr, FI_TYPE_FABRIC_ATTR));
  LCI_Log(LOG_INFO, "ofi", "Domain attributes: %s\n",
          fi_tostr(ofi_info->domain_attr, FI_TYPE_DOMAIN_ATTR));
  LCI_Log(LOG_INFO, "ofi", "Endpoint attributes: %s\n",
          fi_tostr(ofi_info->ep_attr, FI_TYPE_EP_ATTR));
  LCI_Assert(ofi_info->domain_attr->cq_data_size >= 4,
             "cq_data_size (%lu) is too small!\n",
             ofi_info->domain_attr->cq_data_size);
  LCI_Assert(ofi_info->domain_attr->mr_key_size <= 8,
             "mr_key_size (%lu) is too large!\n",
             ofi_info->domain_attr->mr_key_size);
  fi_freeinfo(hints);
  if (ofi_info->tx_attr->inject_size < attr.max_inject_size) {
    LCI_Log(LOG_INFO, "ofi",
            "Reduce max_inject_size to %lu "
            "as required by the libfabric inject_size attribute\n",
            ofi_info->tx_attr->inject_size);
    attr.max_inject_size = ofi_info->tx_attr->inject_size;
  }

  // Create libfabric obj.
  FI_SAFECALL(fi_fabric(ofi_info->fabric_attr, &ofi_fabric, nullptr));

  if (ofi_info->ep_attr->max_msg_size < attr.max_msg_size) {
    attr.max_msg_size = ofi_info->ep_attr->max_msg_size;
    LCI_Log(LOG_INFO, "ofi",
            "Reduce max_msg_size to %lu "
            "as required by the libfabric max_msg_size attribute\n",
            attr.max_msg_size);
  }
}

ofi_net_context_impl_t::~ofi_net_context_impl_t()
{
  FI_SAFECALL(fi_close((struct fid*)&ofi_fabric->fid));
  fi_freeinfo(ofi_info);
}

device_t ofi_net_context_impl_t::alloc_device(device_t::attr_t attr)
{
  device_t ret;
  ret.p_impl = new ofi_device_impl_t(net_context, attr);
  return ret;
}

std::atomic<uint64_t> ofi_device_impl_t::g_next_rdma_key(0);

ofi_device_impl_t::ofi_device_impl_t(net_context_t context_,
                                     device_t::attr_t attr_)
    : device_impl_t(context_, attr_), ofi_lock_mode(attr.ofi_lock_mode)
{
  auto p_ofi_context = static_cast<ofi_net_context_impl_t*>(net_context.p_impl);
  ofi_domain_attr = p_ofi_context->ofi_info->domain_attr;
  // Create domain.
  FI_SAFECALL(fi_domain(p_ofi_context->ofi_fabric, p_ofi_context->ofi_info,
                        &ofi_domain, nullptr));

  // Create end-point;
  if (p_ofi_context->ofi_info->tx_attr->size < attr.net_max_sends) {
    LCI_Log(LOG_INFO, "ofi",
            "Reduce net_max_sends to %lu "
            "as required by the libfabric tx_attr size\n",
            attr.net_max_sends);
    attr.net_max_sends = p_ofi_context->ofi_info->tx_attr->size;
  }
  if (p_ofi_context->ofi_info->rx_attr->size < attr.net_max_recvs) {
    LCI_Log(LOG_INFO, "ofi",
            "Reduce net_max_recvs to %lu "
            "as required by the libfabric rx_attr size\n",
            attr.net_max_recvs);
    attr.net_max_recvs = p_ofi_context->ofi_info->rx_attr->size;
  }
  p_ofi_context->ofi_info->tx_attr->size = attr.net_max_sends;
  p_ofi_context->ofi_info->rx_attr->size = attr.net_max_recvs;
  FI_SAFECALL(
      fi_endpoint(ofi_domain, p_ofi_context->ofi_info, &ofi_ep, nullptr));

  // Create cq.
  struct fi_cq_attr cq_attr;
  memset(&cq_attr, 0, sizeof(struct fi_cq_attr));
  cq_attr.format = FI_CQ_FORMAT_DATA;
  cq_attr.size = attr.net_max_cqes;
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
  LCI_Log(LOG_INFO, "ofi", "addrlen = %lu\n", addrlen);
  LCI_Assert(addrlen <= 8 * EP_ADDR_LEN, "addrlen = %lu\n", addrlen);
  uint64_t my_addr[EP_ADDR_LEN];
  FI_SAFECALL(fi_getname((fid_t)ofi_ep, my_addr, &addrlen));

  int rank = get_rank();
  int nranks = get_nranks();
  peer_addrs.resize(nranks);
  // char key[LCT_PMI_STRING_LIMIT + 1];
  // sprintf(key, "LCI_KEY_%d_%d", attr.uid, rank);
  // char value[LCT_PMI_STRING_LIMIT + 1];
  // const char* PARSE_STRING = "%016lx-%016lx-%016lx-%016lx-%016lx-%016lx";
  // sprintf(value, PARSE_STRING, my_addr[0], my_addr[1], my_addr[2],
  // my_addr[3], my_addr[4], my_addr[5]);
  // LCT_pmi_publish(key, value);
  // LCT_pmi_barrier();
  struct bootstrap_data_t {
    int source_rank;
    int uid;
    uint64_t addr[EP_ADDR_LEN];
  };
  bootstrap_data_t data;
  data.source_rank = rank;
  data.uid = attr.uid;
  memcpy(data.addr, my_addr, sizeof(my_addr));
  std::vector<bootstrap_data_t> bootstrap_datav_in(nranks, data);
  std::vector<bootstrap_data_t> bootstrap_datav_out(nranks);

  bootstrap::alltoall(bootstrap_datav_in.data(), bootstrap_datav_out.data(),
                      sizeof(bootstrap_data_t));

  for (int i = 0; i < nranks; i++) {
    // sprintf(key, "LCI_KEY_%d_%d", attr.uid, i);
    // LCT_pmi_getname(i, key, value);
    // uint64_t peer_addr[EP_ADDR_LEN];

    // sscanf(value, PARSE_STRING, &peer_addr[0], &peer_addr[1], &peer_addr[2],
    //  &peer_addr[3], &peer_addr[4], &peer_addr[5]);
    bootstrap_data_t data = bootstrap_datav_out[i];
    LCI_Assert(data.source_rank == i,
               "Unexpected source rank %d, expected %d\n", data.source_rank, i);
    LCI_Assert(data.uid == attr.uid, "Unexpected uid %d, expected %d\n",
               data.uid, attr.uid);
    uint64_t peer_addr[EP_ADDR_LEN];
    memcpy(peer_addr, data.addr, sizeof(peer_addr));
    int ret =
        fi_av_insert(ofi_av, (void*)peer_addr, 1, &peer_addrs[i], 0, nullptr);
    LCI_Assert(ret == 1, "fi_av_insert failed! ret = %d\n", ret);
  }
  // LCT_pmi_barrier();
}

ofi_device_impl_t::~ofi_device_impl_t()
{
  // LCT_pmi_barrier();
  unbind_packet_pool();
  FI_SAFECALL(fi_close((struct fid*)&ofi_ep->fid));
  FI_SAFECALL(fi_close((struct fid*)&ofi_cq->fid));
  FI_SAFECALL(fi_close((struct fid*)&ofi_av->fid));
  FI_SAFECALL(fi_close((struct fid*)&ofi_domain->fid));
}

endpoint_t ofi_device_impl_t::alloc_endpoint_impl(endpoint_t::attr_t attr)
{
  endpoint_t ret;
  ret.p_impl = new ofi_endpoint_impl_t(device, attr);
  return ret;
}

mr_t ofi_device_impl_t::register_memory_impl(void* buffer, size_t size)
{
  uint64_t rdma_key;
  if (ofi_domain_attr->mr_mode & FI_MR_PROV_KEY) {
    rdma_key = 0;
  } else {
    rdma_key = g_next_rdma_key++;
  }

  mr_t ret;
  ret.p_impl = new ofi_mr_impl_t();

  struct fid_mr* ofi_mr;
  FI_SAFECALL(fi_mr_reg(
      ofi_domain, buffer, size,
      FI_SEND | FI_RECV | FI_READ | FI_WRITE | FI_REMOTE_WRITE | FI_REMOTE_READ,
      0, rdma_key, 0, &ofi_mr, 0));
  if (ofi_domain_attr->mr_mode & FI_MR_ENDPOINT) {
    FI_SAFECALL(fi_mr_bind(ofi_mr, &ofi_ep->fid, 0));
    FI_SAFECALL(fi_mr_enable(ofi_mr));
  }
  static_cast<ofi_mr_impl_t*>(ret.p_impl)->ofi_mr = ofi_mr;
  return ret;
}

void ofi_device_impl_t::deregister_memory_impl(mr_impl_t* mr_impl)
{
  auto p_ofi_mr = static_cast<ofi_mr_impl_t*>(mr_impl);
  FI_SAFECALL(fi_close(&p_ofi_mr->ofi_mr->fid));
  delete p_ofi_mr;
}

ofi_endpoint_impl_t::ofi_endpoint_impl_t(device_t device_, attr_t attr_)
    : endpoint_impl_t(device_, attr_),
      p_ofi_device(reinterpret_cast<ofi_device_impl_t*>(device.p_impl)),
      ofi_domain_attr(p_ofi_device->ofi_domain_attr),
      ofi_ep(p_ofi_device->ofi_ep),
      peer_addrs(p_ofi_device->peer_addrs),
      ofi_lock_mode(p_ofi_device->ofi_lock_mode),
      lock(p_ofi_device->lock)
{
  my_rank = get_rank();
}

ofi_endpoint_impl_t::~ofi_endpoint_impl_t() {}
}  // namespace lci