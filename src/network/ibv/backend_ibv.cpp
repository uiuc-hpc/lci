// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"
#include "network/ibv/backend_ibv_detail.hpp"

namespace lci
{
namespace
{
const int max_sge_num = 1;

const char* mtu_str(enum ibv_mtu mtu)
{
  switch (mtu) {
    case IBV_MTU_256:
      return "256";
    case IBV_MTU_512:
      return "512";
    case IBV_MTU_1024:
      return "1024";
    case IBV_MTU_2048:
      return "2048";
    case IBV_MTU_4096:
      return "4096";
    default:
      return "invalid MTU";
  }
}
}  // namespace

ibv_net_context_impl_t::ibv_net_context_impl_t(runtime_t runtime_, attr_t attr_)
    : net_context_impl_t(runtime_, attr_)
{
  int num_devices;
  ib_dev_list = ibv_get_device_list(&num_devices);
  if (num_devices <= 0) {
    fprintf(stderr, "Unable to find any IB devices\n");
    exit(EXIT_FAILURE);
  }

  bool ret = ibv_detail::select_best_device_port(ib_dev_list, num_devices,
                                                 &ib_dev, &ib_dev_port);
  LCI_Assert(ret, "Cannot find available ibv device/port!\n");

  // ibv_open_device provides the user with a verbs context which is the object
  // that will be used for all other verb operations.
  ib_context = ibv_open_device(ib_dev);
  if (!ib_context) {
    fprintf(stderr, "Couldn't get context for %s\n",
            ibv_get_device_name(ib_dev));
    exit(EXIT_FAILURE);
  }

  // allocate protection domain
  ib_pd = ibv_alloc_pd(ib_context);
  if (!ib_pd) {
    fprintf(stderr, "Could not create protection domain for context\n");
    exit(EXIT_FAILURE);
  }

  // query device attribute
  int rc = ibv_query_device(ib_context, &ib_dev_attr);
  if (rc != 0) {
    fprintf(stderr, "Unable to query device\n");
    exit(EXIT_FAILURE);
  }

  if (ibv_query_device_ex(ib_context, nullptr, &ib_dev_attrx)) {
    fprintf(stderr, "Unable to query device for its extended features\n");
    exit(EXIT_FAILURE);
  }

  // configure on-demand paging
  ib_odp_mr = nullptr;
  if (attr.ibv_odp_strategy == attr_ibv_odp_strategy_t::implicit_odp) {
    const uint32_t rc_caps_mask = IBV_ODP_SUPPORT_SEND | IBV_ODP_SUPPORT_RECV |
                                  IBV_ODP_SUPPORT_WRITE | IBV_ODP_SUPPORT_READ |
                                  IBV_ODP_SUPPORT_SRQ_RECV;
    if (!(ib_dev_attrx.odp_caps.general_caps & IBV_ODP_SUPPORT) ||
        (ib_dev_attrx.odp_caps.per_transport_caps.rc_odp_caps & rc_caps_mask) !=
            rc_caps_mask) {
      fprintf(stderr, "The device isn't ODP capable\n");
      exit(EXIT_FAILURE);
    }
    if (!(ib_dev_attrx.odp_caps.general_caps & IBV_ODP_SUPPORT_IMPLICIT)) {
      fprintf(stderr, "The device doesn't support implicit ODP\n");
      exit(EXIT_FAILURE);
    }
    ib_odp_mr = ibv_reg_mr(ib_pd, nullptr, SIZE_MAX,
                           IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                               IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_ON_DEMAND);
    if (!ib_odp_mr) {
      fprintf(stderr, "Couldn't register MR\n");
      exit(EXIT_FAILURE);
    }
  }

  // query port attribute
  uint8_t dev_port = 0;
  for (; dev_port < 128; dev_port++) {
    rc = ibv_query_port(ib_context, dev_port, &ib_port_attr);
    if (rc == 0) {
      break;
    }
  }
  if (rc != 0) {
    fprintf(stderr, "Unable to query port\n");
    exit(EXIT_FAILURE);
  } else if (ib_port_attr.link_layer == IBV_LINK_LAYER_INFINIBAND &&
             !ib_port_attr.lid) {
    fprintf(stderr, "Couldn't get local LID\n");
    exit(EXIT_FAILURE);
  }
  ib_dev_port = dev_port;
  LCI_Log(LOG_INFO, "ibv", "Maximum MTU: %s; Active MTU: %s\n",
          mtu_str(ib_port_attr.max_mtu), mtu_str(ib_port_attr.active_mtu));

  // Check max_msg_sz
  if (attr.max_msg_size > ib_port_attr.max_msg_sz) {
    attr.max_msg_size = ib_port_attr.max_msg_sz;
    LCI_Log(LOG_INFO, "ibv",
            "Reduce LCI_MAX_SINGLE_MESSAGE_SIZE to %lu "
            "as required by libibverbs max message size\n",
            attr.max_msg_size);
  }

  // query the gid
  if (attr.ibv_gid_idx < 0 &&
      (attr.ibv_force_gid_auto_select ||
       ib_port_attr.link_layer == IBV_LINK_LAYER_ETHERNET)) {
    // User did not explicitly specify the gid to use and we are using RoCE
    attr.ibv_gid_idx = ibv_detail::select_best_gid_for_roce(
        ib_dev, ib_context, ib_pd, ib_port_attr, ib_dev_port);
  }
  if (attr.ibv_gid_idx >= 0) {
    LCI_Log(LOG_INFO, "ibv", "Use GID index: %d\n", attr.ibv_gid_idx);
    if (ibv_query_gid(ib_context, ib_dev_port, attr.ibv_gid_idx, &ib_gid)) {
      fprintf(stderr, "can't read sgid of index %d\n", attr.ibv_gid_idx);
      exit(EXIT_FAILURE);
    }
  } else
    memset(&ib_gid, 0, sizeof(ib_gid));
}

ibv_net_context_impl_t::~ibv_net_context_impl_t()
{
  ibv_free_device_list(ib_dev_list);
  if (ib_odp_mr != nullptr) IBV_SAFECALL(ibv_dereg_mr(ib_odp_mr));
  IBV_SAFECALL(ibv_dealloc_pd(ib_pd));
  IBV_SAFECALL(ibv_close_device(ib_context));
}

device_t ibv_net_context_impl_t::alloc_device(device_t::attr_t attr)
{
  device_t ret;
  ret.p_impl = new ibv_device_impl_t(net_context, attr);
  return ret;
}

ibv_device_impl_t::ibv_device_impl_t(net_context_t net_context_,
                                     device_t::attr_t attr_)
    : device_impl_t(net_context_, attr_)
{
  net_context_attr = net_context.get_attr();
  ibv_net_context_impl_t* p_net_context =
      static_cast<ibv_net_context_impl_t*>(net_context.p_impl);

  odp_mr.ibv_mr = p_net_context->ib_odp_mr;

  // adjust max_send, max_recv, max_cqe
  // Note: the max value reported by device attributes might still not be the
  // strict upper bound.
  if (attr.net_max_sends > p_net_context->ib_dev_attr.max_qp_wr) {
    LCI_Log(LOG_INFO, "ibv",
            "The configured net_max_sends (%d) is adjusted as it is "
            "larger than the maximum allowable value by the device (%d).\n",
            attr.net_max_sends, p_net_context->ib_dev_attr.max_qp_wr);
    attr.net_max_sends = p_net_context->ib_dev_attr.max_qp_wr;
  }
  if (attr.net_max_recvs > p_net_context->ib_dev_attr.max_srq_wr) {
    LCI_Log(LOG_INFO, "ibv",
            "The configured LCI_SERVER_MAX_RECVS (%d) is adjusted as it is "
            "larger than the maximum allowable value by the device (%d).\n",
            attr.net_max_recvs, p_net_context->ib_dev_attr.max_srq_wr);
    attr.net_max_recvs = p_net_context->ib_dev_attr.max_srq_wr;
  }
  if (attr.net_max_cqes > p_net_context->ib_dev_attr.max_cqe) {
    LCI_Log(LOG_INFO, "ibv",
            "The configured LCI_SERVER_MAX_CQES (%d) is adjusted as it is "
            "larger than the maximum allowable value by the device (%d).\n",
            attr.net_max_cqes, p_net_context->ib_dev_attr.max_cqe);
    attr.net_max_cqes = p_net_context->ib_dev_attr.max_cqe;
  }

  // Create shared-receive queue, **number here affect performance**.
  struct ibv_srq_init_attr srq_attr;
  memset(&srq_attr, 0, sizeof(srq_attr));
  srq_attr.srq_context = nullptr;
  srq_attr.attr.max_wr = attr.net_max_recvs;
  srq_attr.attr.max_sge = max_sge_num;
  srq_attr.attr.srq_limit = 0;
  ib_srq = ibv_create_srq(p_net_context->ib_pd, &srq_attr);
  if (!ib_srq) {
    fprintf(stderr, "Could not create shared received queue\n");
    exit(EXIT_FAILURE);
  }

  // Create completion queues.
  ib_cq = ibv_create_cq(p_net_context->ib_context, attr.net_max_cqes, nullptr,
                        nullptr, 0);
  if (!ib_cq) {
    fprintf(stderr, "Unable to create cq\n");
    exit(EXIT_FAILURE);
  }

  ib_pd = nullptr;
  if (net_context_attr.ibv_td_strategy == attr_ibv_td_strategy_t::all_qp) {
    // allocate one thread domain for all queue pairs
    struct ibv_td_init_attr td_attr;
    td_attr.comp_mask = 0;
    ib_td = ibv_alloc_td(p_net_context->ib_context, &td_attr);
    if (ib_td != nullptr) {
      struct ibv_parent_domain_init_attr attr;
      attr.td = ib_td;
      attr.pd = p_net_context->ib_pd;
      attr.comp_mask = 0;
      ib_pd = ibv_alloc_parent_domain(p_net_context->ib_context, &attr);
      if (ib_pd == nullptr) {
        LCI_Log(LOG_INFO, "ibv", "ibv_alloc_parent_domain() failed (%s)\n",
                strerror(errno));
        IBV_SAFECALL(ibv_dealloc_td(ib_td));
      }
    } else {
      LCI_Log(LOG_INFO, "ibv", "ibv_alloc_td() failed (%s)\n", strerror(errno));
    }
  } else if (net_context_attr.ibv_td_strategy ==
             attr_ibv_td_strategy_t::per_qp) {
    // allocate one thread domain for each queue pair
    ib_qp_extras.resize(get_nranks());
    for (int i = 0; i < get_nranks(); ++i) {
      // allocate thread domain
      ib_qp_extras[i].ib_pd = nullptr;
      struct ibv_td_init_attr td_attr;
      td_attr.comp_mask = 0;
      ib_qp_extras[i].ib_td = ibv_alloc_td(p_net_context->ib_context, &td_attr);
      if (ib_qp_extras[i].ib_td != nullptr) {
        struct ibv_parent_domain_init_attr pd_attr;
        pd_attr.td = ib_qp_extras[i].ib_td;
        pd_attr.pd = p_net_context->ib_pd;
        pd_attr.comp_mask = 0;
        ib_qp_extras[i].ib_pd =
            ibv_alloc_parent_domain(p_net_context->ib_context, &pd_attr);
        if (ib_qp_extras[i].ib_pd == nullptr) {
          LCI_Log(LOG_INFO, "ibv", "ibv_alloc_parent_domain() failed (%s)\n",
                  strerror(errno));
          IBV_SAFECALL(ibv_dealloc_td(ib_qp_extras[i].ib_td));
        }
      } else {
        LCI_Log(LOG_INFO, "ibv", "ibv_alloc_td() failed (%s)\n",
                strerror(errno));
      }
      if (ib_qp_extras[i].ib_pd == nullptr) {
        ib_qp_extras[i].ib_td = nullptr;
        ib_qp_extras[i].ib_pd = p_net_context->ib_pd;
      }
    }
  }
  if (ib_pd == nullptr) {
    ib_td = nullptr;
    ib_pd = p_net_context->ib_pd;
  }

  ib_qps.resize(get_nranks());
  for (int i = 0; i < get_nranks(); i++) {
    {
      // Create a queue pair
      struct ibv_qp_init_attr init_attr;
      memset(&init_attr, 0, sizeof(init_attr));
      init_attr.send_cq = ib_cq;
      init_attr.recv_cq = ib_cq;
      init_attr.srq = ib_srq;
      init_attr.cap.max_send_wr = attr.net_max_sends;
      init_attr.cap.max_recv_wr = attr.net_max_recvs;
      init_attr.cap.max_send_sge = max_sge_num;
      init_attr.cap.max_recv_sge = max_sge_num;
      init_attr.cap.max_inline_data = net_context_attr.max_inject_size;
      init_attr.qp_type = IBV_QPT_RC;
      init_attr.sq_sig_all = 0;
      struct ibv_pd* pd = ib_pd;
      if (!ib_qp_extras.empty()) {
        pd = ib_qp_extras[i].ib_pd;
      }
      ib_qps[i] = ibv_create_qp(pd, &init_attr);

      if (!ib_qps[i]) {
        fprintf(stderr, "Couldn't create QP\n");
        exit(EXIT_FAILURE);
      }

      struct ibv_qp_attr qp_attr;
      memset(&qp_attr, 0, sizeof(qp_attr));
      ibv_query_qp(ib_qps[i], &qp_attr, IBV_QP_CAP, &init_attr);
      LCI_Assert(
          qp_attr.cap.max_inline_data >= net_context_attr.max_inject_size,
          "Specified inline size %d is too large (maximum %d)",
          net_context_attr.max_inject_size, init_attr.cap.max_inline_data);
      if (net_context_attr.max_inject_size < qp_attr.cap.max_inline_data) {
        net_context_attr.max_inject_size = qp_attr.cap.max_inline_data;
        LCI_Log(LOG_INFO, "ibv",
                "Maximum inline-size(%d) > requested inline-size(%d)\n",
                qp_attr.cap.max_inline_data, net_context_attr.max_inject_size);
      }
    }
    {
      // When a queue pair (QP) is newly created, it is in the RESET
      // state. The first state transition that needs to happen is to
      // bring the QP in the INIT state.
      struct ibv_qp_attr attr;
      memset(&attr, 0, sizeof(attr));
      attr.qp_state = IBV_QPS_INIT;
      attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                             IBV_ACCESS_REMOTE_WRITE;
      attr.pkey_index = 0;
      attr.port_num = p_net_context->ib_dev_port;

      int flags =
          IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
      int rc = ibv_modify_qp(ib_qps[i], &attr, flags);
      if (rc != 0) {
        fprintf(stderr, "Failed to modify QP to INIT\n");
        exit(EXIT_FAILURE);
      }
    }
    char wgid[ibv_detail::WIRE_GID_NBYTES + 1];
    memset(wgid, 0, sizeof(wgid));
    ibv_detail::gid_to_wire_gid(&p_net_context->ib_gid, wgid);
    // Use this queue pair "i" to connect to rank e.
    char key[LCT_PMI_STRING_LIMIT + 1];
    sprintf(key, "LCI_KEY_%d_%d_%d", get_id(), get_rank(), i);
    char value[LCT_PMI_STRING_LIMIT + 1];
    sprintf(value, "%x:%hx:%s", ib_qps[i]->qp_num,
            p_net_context->ib_port_attr.lid, wgid);
    LCT_pmi_publish(key, value);
  }
  LCI_Log(LOG_INFO, "ibv", "Current inline data size is %d\n",
          net_context_attr.max_inject_size);
  LCT_pmi_barrier();

  for (int i = 0; i < get_nranks(); i++) {
    char key[LCT_PMI_STRING_LIMIT + 1];
    sprintf(key, "LCI_KEY_%d_%d_%d", get_id(), i, get_rank());
    char value[LCT_PMI_STRING_LIMIT + 1];
    LCT_pmi_getname(i, key, value);
    uint32_t dest_qpn;
    uint16_t dest_lid;
    union ibv_gid gid;
    char wgid[ibv_detail::WIRE_GID_NBYTES + 1];
    sscanf(value, "%x:%hx:%s", &dest_qpn, &dest_lid, wgid);
    ibv_detail::wire_gid_to_gid(wgid, &gid);
    // Once a queue pair (QP) has receive buffers posted to it, it is now
    // possible to transition the QP into the ready to receive (RTR) state.
    {
      struct ibv_qp_attr attr;
      memset(&attr, 0, sizeof(attr));
      attr.qp_state = IBV_QPS_RTR;
      attr.path_mtu = p_net_context->ib_port_attr.active_mtu;
      // starting receive packet sequence number
      // (should match remote QP's sq_psn)
      attr.rq_psn = 0;
      attr.dest_qp_num = dest_qpn;
      // an address handle (AH) needs to be created and filled in as
      // appropriate. Minimally; ah_attr.dlid needs to be filled in.
      attr.ah_attr.dlid = dest_lid;
      attr.ah_attr.sl = 0;
      attr.ah_attr.src_path_bits = 0;
      attr.ah_attr.is_global = 0;
      attr.ah_attr.static_rate = 0;
      attr.ah_attr.port_num = p_net_context->ib_dev_port;
      // maximum number of resources for incoming RDMA requests
      // don't know what this is
      attr.max_dest_rd_atomic = 1;
      // minimum RNR NAK timer (recommended value: 12)
      attr.min_rnr_timer = 12;
      // should not be necessary to set these, given is_global = 0
      memset(&attr.ah_attr.grh, 0, sizeof attr.ah_attr.grh);
      // If we are using gid
      if (gid.global.interface_id) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = gid;
        attr.ah_attr.grh.sgid_index = net_context_attr.ibv_gid_idx;
      }

      int flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                  IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                  IBV_QP_MIN_RNR_TIMER;

      int rc = ibv_modify_qp(ib_qps[i], &attr, flags);
      if (rc != 0) {
        fprintf(stderr, "failed to modify QP state to RTR\n");
        exit(EXIT_FAILURE);
      }
    }
    // Once a queue pair (QP) has reached ready to receive (RTR) state,
    // it may then be transitioned to the ready to send (RTS) state.
    {
      struct ibv_qp_attr attr;
      memset(&attr, 0, sizeof(attr));
      attr.qp_state = IBV_QPS_RTS;
      attr.sq_psn = 0;
      // number of outstanding RDMA reads and atomic operations allowed
      attr.max_rd_atomic = 1;
      // "Failed status transport retry counter exceeded (12) for wr_id"
      attr.timeout = 18;
      attr.retry_cnt = 7;
      attr.rnr_retry = 7;

      int flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                  IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
      int rc = ibv_modify_qp(ib_qps[i], &attr, flags);
      if (rc != 0) {
        fprintf(stderr, "failed to modify QP state to RTS\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  int j = get_nranks();
  int* b;
  while (j < INT32_MAX) {
    b = (int*)calloc(j, sizeof(int));
    int i = 0;
    for (; i < get_nranks(); i++) {
      int k = (ib_qps[i]->qp_num % j);
      if (b[k]) break;
      b[k] = 1;
    }
    if (i == get_nranks()) break;
    j++;
    free(b);
  }
  LCI_Assert(j != INT32_MAX,
             "Cannot find a suitable mod to hold qp2rank map\n");
  for (int i = 0; i < get_nranks(); i++) {
    b[ib_qps[i]->qp_num % j] = i;
  }
  qp2rank_mod = j;
  qp2rank = b;
  LCI_Log(LOG_INFO, "ibv", "qp2rank_mod is %d\n", j);
  LCT_pmi_barrier();
}

ibv_device_impl_t::~ibv_device_impl_t()
{
  LCT_pmi_barrier();
  unbind_packet_pool();
  free(qp2rank);
  for (int i = 0; i < get_nranks(); i++) {
    IBV_SAFECALL(ibv_destroy_qp(ib_qps[i]));
  }
  if (!ib_qp_extras.empty()) {
    for (int i = 0; i < get_nranks(); ++i) {
      if (ib_qp_extras[i].ib_td) {
        IBV_SAFECALL(ibv_dealloc_pd(ib_qp_extras[i].ib_pd));
        IBV_SAFECALL(ibv_dealloc_td(ib_qp_extras[i].ib_td));
      }
    }
  }
  if (ib_td) {
    IBV_SAFECALL(ibv_dealloc_pd(ib_pd));
    IBV_SAFECALL(ibv_dealloc_td(ib_td));
  }
  IBV_SAFECALL(ibv_destroy_cq(ib_cq));
  IBV_SAFECALL(ibv_destroy_srq(ib_srq));
}

endpoint_t ibv_device_impl_t::alloc_endpoint_impl(endpoint_t::attr_t attr)
{
  endpoint_t ret;
  ret.p_impl = new ibv_endpoint_impl_t(device, attr);
  return ret;
}

mr_t ibv_device_impl_t::register_memory_impl(void* buffer, size_t size)
{
  ibv_mr_impl_t* mr;

  if (net_context_attr.ibv_odp_strategy ==
      attr_ibv_odp_strategy_t::implicit_odp) {
    mr = &odp_mr;
  } else {
    mr = new ibv_mr_impl_t();
    int mr_flags;
    if (net_context_attr.ibv_odp_strategy ==
        attr_ibv_odp_strategy_t::explicit_odp) {
      mr_flags = IBV_ACCESS_ON_DEMAND | IBV_ACCESS_LOCAL_WRITE |
                 IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    } else {
      mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                 IBV_ACCESS_REMOTE_WRITE;
    }
    mr->ibv_mr = ibv_reg_mr(ib_pd, buffer, size, mr_flags);
    if (net_context_attr.ibv_prefetch_strategy !=
        attr_ibv_prefetch_strategy_t::none) {
      struct ibv_sge list = {
          .addr = (uintptr_t)buffer,
          .length = (uint32_t)size,
          .lkey = ibv_detail::get_mr_lkey(mr),
      };
      enum ibv_advise_mr_advice advice;
      switch (net_context_attr.ibv_prefetch_strategy) {
        case attr_ibv_prefetch_strategy_t::prefetch:
          advice = IBV_ADVISE_MR_ADVICE_PREFETCH;
          break;
        case attr_ibv_prefetch_strategy_t::prefetch_write:
          advice = IBV_ADVISE_MR_ADVICE_PREFETCH_WRITE;
          break;
        case attr_ibv_prefetch_strategy_t::prefetch_no_fault:
          advice = IBV_ADVISE_MR_ADVICE_PREFETCH_NO_FAULT;
          break;
        default:
          LCI_Assert(false, "Invalid prefetch strategy\n");
      }
      IBV_SAFECALL(ibv_advise_mr(ib_pd, advice, 0, &list, 1));
    }
  }
  mr_t ret;
  ret.p_impl = mr;
  return ret;
}

void ibv_device_impl_t::deregister_memory_impl(mr_impl_t* mr_impl)
{
  if (net_context_attr.ibv_odp_strategy ==
      attr_ibv_odp_strategy_t::implicit_odp) {
    return;
  } else {
    auto p_ibv_mr = static_cast<ibv_mr_impl_t*>(mr_impl);
    IBV_SAFECALL(ibv_dereg_mr(p_ibv_mr->ibv_mr));
    delete p_ibv_mr;
  }
}

ibv_endpoint_impl_t::ibv_endpoint_impl_t(device_t device_, attr_t attr_)
    : endpoint_impl_t(device_, attr_),
      p_ibv_device(reinterpret_cast<ibv_device_impl_t*>(device.p_impl)),
      ib_qps(p_ibv_device->ib_qps),
      ib_qp_extras(p_ibv_device->ib_qp_extras),
      net_context_attr(p_ibv_device->net_context_attr)
{
  auto p_ibv_context = reinterpret_cast<ibv_net_context_impl_t*>(
      p_ibv_device->net_context.p_impl);
}

ibv_endpoint_impl_t::~ibv_endpoint_impl_t() {}

}  // namespace lci