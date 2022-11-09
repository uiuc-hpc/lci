#include "lcii.h"

static const int max_sge_num = 1;
static const int inline_size = 236;

static int g_device_num = 0;

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

static inline void* LCISI_event_polling_thread_fn(void* argp)
{
  LCISI_server_t* server = (LCISI_server_t*)argp;
  LCM_Log(LCM_LOG_INFO, "event", "Start ibv event polling thread!\n");
  struct ibv_async_event event;
  while (server->event_polling_thread_run) {
    IBV_SAFECALL(ibv_get_async_event(server->dev_ctx, &event));
    switch (event.event_type) {
      case IBV_EVENT_CQ_ERR:
      case IBV_EVENT_QP_FATAL:
      case IBV_EVENT_QP_REQ_ERR:
      case IBV_EVENT_QP_ACCESS_ERR:
      case IBV_EVENT_PATH_MIG_ERR:
      case IBV_EVENT_DEVICE_FATAL:
      case IBV_EVENT_SRQ_ERR:
        LCM_Assert(false, "Got ibv async event error %d: %s\n",
                   event.event_type, ibv_event_type_str(event.event_type));
        break;
      default:
        LCM_Log(LCM_LOG_INFO, "event", "Got ibv async event %d: %s\n",
                event.event_type, ibv_event_type_str(event.event_type));
    }
    ibv_ack_async_event(&event);
  }
  LCM_Log(LCM_LOG_INFO, "event", "End ibv event polling thread!\n");
}

void LCISI_event_polling_thread_init(LCISI_server_t* server)
{
  if (LCI_IBV_ENABLE_EVENT_POLLING_THREAD) {
    server->event_polling_thread_run = true;
    pthread_create(&server->event_polling_thread, NULL,
                   LCISI_event_polling_thread_fn, server);
  }
}

void LCISI_event_polling_thread_fina(LCISI_server_t* server)
{
  if (LCI_IBV_ENABLE_EVENT_POLLING_THREAD) {
    server->event_polling_thread_run = false;
    pthread_join(server->event_polling_thread, NULL);
  }
}

void LCISD_init(LCI_device_t device, LCIS_server_t* s)
{
  int device_id = g_device_num++;
  LCISI_server_t* server = LCIU_malloc(sizeof(LCISI_server_t));
  *s = (LCIS_server_t)server;
  server->device = device;

  int num_devices;
  server->dev_list = ibv_get_device_list(&num_devices);
  if (num_devices <= 0) {
    fprintf(stderr, "Unable to find any IB devices\n");
    exit(EXIT_FAILURE);
  }

  // Use the last one by default.
  server->ib_dev = server->dev_list[num_devices - 1];
  if (!server->ib_dev) {
    fprintf(stderr, "No IB devices found\n");
    exit(EXIT_FAILURE);
  }
  LCM_Log(LCM_LOG_INFO, "ibv", "Use IB server: %s\n",
          ibv_get_device_name(server->ib_dev));

  // ibv_open_device provides the user with a verbs context which is the object
  // that will be used for all other verb operations.
  server->dev_ctx = ibv_open_device(server->ib_dev);
  if (!server->dev_ctx) {
    fprintf(stderr, "Couldn't get context for %s\n",
            ibv_get_device_name(server->ib_dev));
    exit(EXIT_FAILURE);
  }

  // allocate protection domain
  server->dev_pd = ibv_alloc_pd(server->dev_ctx);
  if (!server->dev_pd) {
    fprintf(stderr, "Could not create protection domain for context\n");
    exit(EXIT_FAILURE);
  }

  // query device attribute
  int rc = ibv_query_device(server->dev_ctx, &server->dev_attr);
  if (rc != 0) {
    fprintf(stderr, "Unable to query device\n");
    exit(EXIT_FAILURE);
  }

  if (ibv_query_device_ex(server->dev_ctx, NULL, &server->dev_attrx)) {
    fprintf(stderr, "Unable to query device for its extended features\n");
    exit(EXIT_FAILURE);
  }

  server->odp_mr = NULL;
  if (LCI_IBV_USE_ODP == 2) {
    const uint32_t rc_caps_mask = IBV_ODP_SUPPORT_SEND | IBV_ODP_SUPPORT_RECV |
                                  IBV_ODP_SUPPORT_WRITE | IBV_ODP_SUPPORT_READ |
                                  IBV_ODP_SUPPORT_SRQ_RECV;
    if (!(server->dev_attrx.odp_caps.general_caps & IBV_ODP_SUPPORT) ||
        (server->dev_attrx.odp_caps.per_transport_caps.rc_odp_caps &
         rc_caps_mask) != rc_caps_mask) {
      fprintf(stderr, "The device isn't ODP capable\n");
      exit(EXIT_FAILURE);
    }
    if (!(server->dev_attrx.odp_caps.general_caps & IBV_ODP_SUPPORT_IMPLICIT)) {
      fprintf(stderr, "The device doesn't support implicit ODP\n");
      exit(EXIT_FAILURE);
    }
    server->odp_mr =
        ibv_reg_mr(server->dev_pd, NULL, SIZE_MAX,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                       IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_ON_DEMAND);
    if (!server->odp_mr) {
      fprintf(stderr, "Couldn't register MR\n");
      exit(EXIT_FAILURE);
    }
  }

  // query port attribute
  uint8_t dev_port = 0;
  for (; dev_port < 128; dev_port++) {
    rc = ibv_query_port(server->dev_ctx, dev_port, &server->port_attr);
    if (rc == 0) {
      break;
    }
  }
  if (rc != 0) {
    fprintf(stderr, "Unable to query port\n");
    exit(EXIT_FAILURE);
  } else if (server->port_attr.link_layer != IBV_LINK_LAYER_ETHERNET &&
             !server->port_attr.lid) {
    fprintf(stderr, "Couldn't get local LID\n");
    exit(EXIT_FAILURE);
  }
  server->dev_port = dev_port;
  LCM_Log(LCM_LOG_INFO, "ibv", "Maximum MTU: %s; Active MTU: %s\n",
          mtu_str(server->port_attr.max_mtu),
          mtu_str(server->port_attr.active_mtu));

  // Create shared-receive queue, **number here affect performance**.
  struct ibv_srq_init_attr srq_attr;
  memset(&srq_attr, 0, sizeof(srq_attr));
  srq_attr.srq_context = NULL;
  srq_attr.attr.max_wr = LCI_SERVER_MAX_RECVS;
  srq_attr.attr.max_sge = max_sge_num;
  srq_attr.attr.srq_limit = 0;
  server->dev_srq = ibv_create_srq(server->dev_pd, &srq_attr);
  if (!server->dev_srq) {
    fprintf(stderr, "Could not create shared received queue\n");
    exit(EXIT_FAILURE);
  }

  // Create completion queues.
  server->cq =
      ibv_create_cq(server->dev_ctx, LCI_SERVER_MAX_CQES, NULL, NULL, 0);
  if (!server->cq) {
    fprintf(stderr, "Unable to create cq\n");
    exit(EXIT_FAILURE);
  }

  server->qps = LCIU_malloc(LCI_NUM_PROCESSES * sizeof(struct ibv_qp*));

  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    {
      // Create a queue pair
      struct ibv_qp_init_attr init_attr;
      memset(&init_attr, 0, sizeof(init_attr));
      init_attr.send_cq = server->cq;
      init_attr.recv_cq = server->cq;
      init_attr.srq = server->dev_srq;
      init_attr.cap.max_send_wr = LCI_SERVER_MAX_SENDS;
      init_attr.cap.max_recv_wr = LCI_SERVER_MAX_RECVS;
      init_attr.cap.max_send_sge = max_sge_num;
      init_attr.cap.max_recv_sge = max_sge_num;
      init_attr.cap.max_inline_data = inline_size;
      init_attr.qp_type = IBV_QPT_RC;
      init_attr.sq_sig_all = 0;
      server->qps[i] = ibv_create_qp(server->dev_pd, &init_attr);

      if (!server->qps[i]) {
        fprintf(stderr, "Couldn't create QP\n");
        exit(EXIT_FAILURE);
      }

      struct ibv_qp_attr attr;
      memset(&attr, 0, sizeof(attr));
      ibv_query_qp(server->qps[i], &attr, IBV_QP_CAP, &init_attr);
      LCM_Assert(init_attr.cap.max_inline_data >= inline_size,
                 "Specified inline size %d is too large (maximum %d)",
                 inline_size, init_attr.cap.max_inline_data);
      if (inline_size < attr.cap.max_inline_data) {
        LCM_Log(LCM_LOG_INFO, "ibv",
                "Maximum inline-size(%d) > requested inline-size(%d)\n",
                attr.cap.max_inline_data, inline_size);
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
      attr.port_num = server->dev_port;

      int flags =
          IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
      rc = ibv_modify_qp(server->qps[i], &attr, flags);
      if (rc != 0) {
        fprintf(stderr, "Failed to modify QP to INIT\n");
        exit(EXIT_FAILURE);
      }
    }
    // Use this queue pair "i" to connect to rank e.
    char key[256];
    sprintf(key, "LCI_KEY_%d_%d_%d", device_id, LCI_RANK, i);
    char value[256];
    sprintf(value, "%x:%hx", server->qps[i]->qp_num, server->port_attr.lid);
    lcm_pm_publish(key, value);
  }
  LCM_Log(LCM_LOG_INFO, "ibv", "Current inline data size is %d\n", inline_size);
  server->max_inline = inline_size;
  lcm_pm_barrier();

  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    char key[256];
    sprintf(key, "LCI_KEY_%d_%d_%d", device_id, i, LCI_RANK);
    char value[256];
    lcm_pm_getname(key, value);
    uint32_t dest_qpn;
    uint16_t dest_lid;
    sscanf(value, "%x:%hx", &dest_qpn, &dest_lid);
    // Once a queue pair (QP) has receive buffers posted to it, it is now
    // possible to transition the QP into the ready to receive (RTR) state.
    {
      struct ibv_qp_attr attr;
      memset(&attr, 0, sizeof(attr));
      attr.qp_state = IBV_QPS_RTR;
      attr.path_mtu = server->port_attr.active_mtu;
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
      attr.ah_attr.port_num = server->dev_port;
      // maximum number of resources for incoming RDMA requests
      // don't know what this is
      attr.max_dest_rd_atomic = 1;
      // minimum RNR NAK timer (recommended value: 12)
      attr.min_rnr_timer = 12;
      // should not be necessary to set these, given is_global = 0
      memset(&attr.ah_attr.grh, 0, sizeof attr.ah_attr.grh);

      int flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                  IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                  IBV_QP_MIN_RNR_TIMER;

      rc = ibv_modify_qp(server->qps[i], &attr, flags);
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
      attr.timeout = 14;
      attr.retry_cnt = 7;
      attr.rnr_retry = 7;

      int flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                  IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
      rc = ibv_modify_qp(server->qps[i], &attr, flags);
      if (rc != 0) {
        fprintf(stderr, "failed to modify QP state to RTS\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  int j = LCI_NUM_PROCESSES;
  int* b;
  while (j < INT32_MAX) {
    b = (int*)calloc(j, sizeof(int));
    int i = 0;
    for (; i < LCI_NUM_PROCESSES; i++) {
      int k = (server->qps[i]->qp_num % j);
      if (b[k]) break;
      b[k] = 1;
    }
    if (i == LCI_NUM_PROCESSES) break;
    j++;
    free(b);
  }
  LCM_Assert(j != INT32_MAX,
             "Cannot find a suitable mod to hold qp2rank map\n");
  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    b[server->qps[i]->qp_num % j] = i;
  }
  server->qp2rank_mod = j;
  server->qp2rank = b;
  LCM_Log(LCM_LOG_INFO, "ibv", "qp2rank_mod is %d\n", j);

  LCISI_event_polling_thread_init(server);
  lcm_pm_barrier();
}

void LCISD_finalize(LCIS_server_t s)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  LCISI_event_polling_thread_fina(server);
  free(server->qp2rank);
  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    IBV_SAFECALL(ibv_destroy_qp(server->qps[i]));
  }
  LCIU_free(server->qps);
  IBV_SAFECALL(ibv_destroy_cq(server->cq));
  IBV_SAFECALL(ibv_destroy_srq(server->dev_srq));
  ibv_free_device_list(server->dev_list);
  if (server->odp_mr != NULL) IBV_SAFECALL(ibv_dereg_mr(server->odp_mr));
  IBV_SAFECALL(ibv_dealloc_pd(server->dev_pd));
  IBV_SAFECALL(ibv_close_device(server->dev_ctx));
  LCIU_free(server);
}