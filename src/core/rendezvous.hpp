#ifndef LCI_CORE_RENDEZVOUS_HPP
#define LCI_CORE_RENDEZVOUS_HPP

namespace lci
{
struct rts_msg_t {
  rdv_type_t rdv_type : 2;
  int count : 30;
  uintptr_t send_ctx;
  rcomp_t rcomp;
  tag_t tag;
  size_t size_p[0];

  static size_t get_size(data_t data)
  {
    int count = 1;
    if (data.is_buffers()) {
      count = data.get_buffers_count();
    }
    return offsetof(rts_msg_t, size_p) + count * sizeof(size_t);
  }

  void load_buffer(size_t size)
  {
    count = 1;
    this->size_p[0] = size;
  }

  void load_buffers(buffers_t buffers)
  {
    count = buffers.size();
    for (int i = 0; i < count; i++) {
      size_p[i] = buffers[i].size;
    }
  }

  data_t alloc_data()
  {
    if (count == 1) {
      return data_t(size_p[0]);
    } else {
      return data_t(size_p, count);
    }
  }
};

struct rtr_rbuffer_info_t {
  rkey_t rkey;
  uintptr_t remote_addr_base;
  uint64_t remote_addr_offset;
};

inline void fill_rtr_rbuffer_info(rtr_rbuffer_info_t* p, void* buffer, mr_t mr)
{
  p->remote_addr_base = (uintptr_t)mr.get_impl()->address;
  p->remote_addr_offset = (uintptr_t)buffer - p->remote_addr_base;
  p->rkey = get_rkey(mr);
}

struct rtr_msg_t {
  rdv_type_t rdv_type : 2;
  int count : 30;
  uintptr_t send_ctx;
  union {
    // When using writeimm protocol
    uint32_t
        recv_ctx_key; /* the id of the related context on the target side */
    // when using write protocol
    uintptr_t recv_ctx;
  };
  rtr_rbuffer_info_t rbuffer_info_p[0];

  size_t get_size()
  {
    return offsetof(rtr_msg_t, rbuffer_info_p) +
           count * sizeof(rtr_rbuffer_info_t);
  }

  void load_data(const data_t& data)
  {
    LCI_Assert(!data.is_scalar(), "Unexpected scalar data\n");
    if (data.is_buffer()) {
      rdv_type = rdv_type_t::single_1sided;
      fill_rtr_rbuffer_info(&rbuffer_info_p[0], data.buffer.base,
                            data.buffer.mr);
      count = 1;
    } else {
      rdv_type = rdv_type_t::multiple;
      for (int i = 0; i < data.buffers.count; i++) {
        fill_rtr_rbuffer_info(&rbuffer_info_p[i], data.buffers.buffers[i].base,
                              data.buffers.buffers[i].mr);
      }
      count = data.buffers.count;
    }
  }
};

inline void handle_rdv_rts(runtime_t runtime, net_endpoint_t net_endpoint,
                           packet_t* packet, int src_rank,
                           internal_context_t* rdv_ctx, bool is_in_progress)
{
  net_device_t net_device = net_endpoint.get_impl()->net_device;
  // Extract information from the received RTS packet
  rts_msg_t* rts = reinterpret_cast<rts_msg_t*>(packet->payload);
  rdv_type_t rdv_type = rts->rdv_type;
  LCI_DBG_Log(LOG_TRACE, "rdv", "handle rts: rdv_type %d\n", rdv_type);
  if (!rdv_ctx) {
    LCI_Assert(rdv_type == rdv_type_t::single_1sided ||
                   rdv_type == rdv_type_t::multiple,
               "");
    rdv_ctx = new internal_context_t;
  }
  if (rdv_type == rdv_type_t::single_2sided) {
    throw std::logic_error("Not implemented");
    // LCI_DBG_Assert(rdv_ctx->data.lbuffer.address == NULL ||
    //                    rdv_ctx->data.lbuffer.length >= packet->data.rts.size,
    //                "the message sent by sendl (%lu) is larger than the buffer
    //                " "posted by recvl (%lu)!\n", packet->data.rts.size,
    //                rdv_ctx->data.lbuffer.length);
    // LCI_DBG_Assert(packet->context.src_rank == src_rank, "");
    // LCI_DBG_Assert(rdv_ctx->tag == tag, "");
  }
  rdv_ctx->data = rts->alloc_data();
  rdv_ctx->rank = src_rank;
  if (rdv_type == rdv_type_t::single_1sided ||
      rdv_type == rdv_type_t::multiple) {
    // For 2sided, we already set these fields when posting the recvl.
    rdv_ctx->tag = rts->tag;
    rdv_ctx->user_context = NULL;
    rdv_ctx->comp = runtime.p_impl->rcomp_registry.get(rts->rcomp);
    rdv_ctx->rdv_type = rdv_type;
  }

  // Prepare the data
  // if (rdv_type == LCII_RDV_2SIDED) {
  //   if (rdv_ctx->data.lbuffer.address == NULL) {
  //     LCI_lbuffer_alloc(ep->device, packet->data.rts.size,
  //                       &rdv_ctx->data.lbuffer);
  //   }
  //   if (rdv_ctx->data.lbuffer.segment == LCI_SEGMENT_ALL) {
  //     LCI_DBG_Assert(LCII_comp_attr_get_dereg(rdv_ctx->comp_attr) == 1,
  //     "\n"); LCI_memory_register(ep->device, rdv_ctx->data.lbuffer.address,
  //                         rdv_ctx->data.lbuffer.length,
  //                         &rdv_ctx->data.lbuffer.segment);
  //   } else {
  //     LCI_DBG_Assert(LCII_comp_attr_get_dereg(rdv_ctx->comp_attr) == 0,
  //     "\n");
  //   }
  // } else if (rdv_type == LCII_RDV_1SIDED) {
  // allocate a buffer and register it with the endpoint that we are going to
  // use
  register_data(rdv_ctx->data, net_device);
  rdv_ctx->mr_on_the_fly = true;
  // } else {
  //   // rdv_type == LCII_RDV_IOVEC
  //   rdv_ctx->data.iovec.count = packet->data.rts.count;
  //   rdv_ctx->data.iovec.piggy_back.length = packet->data.rts.piggy_back_size;
  //   rdv_ctx->data.iovec.piggy_back.address =
  //       LCIU_malloc(packet->data.rts.piggy_back_size);
  //   memcpy(rdv_ctx->data.iovec.piggy_back.address,
  //          (void*)&packet->data.rts.size_p[packet->data.rts.count],
  //          packet->data.rts.piggy_back_size);
  //   rdv_ctx->data.iovec.lbuffers =
  //       LCIU_malloc(sizeof(LCI_lbuffer_t) * packet->data.rts.count);
  //   for (int i = 0; i < packet->data.rts.count; ++i) {
  //     LCI_lbuffer_alloc(ep->device, packet->data.rts.size_p[i],
  //                       &rdv_ctx->data.iovec.lbuffers[i]);
  //   }
  //   rdv_ctx->data_type = LCI_IOVEC;
  // }

  // Prepare the RTR packet
  // reuse the rts packet as rtr packet
  uintptr_t send_ctx = rts->send_ctx;
  rtr_msg_t* rtr = reinterpret_cast<rtr_msg_t*>(packet->payload);
  packet->local_context.local_id = mpmc_set_t::LOCAL_SET_ID_NULL;
  rtr->send_ctx = send_ctx;
  rtr->rdv_type = rdv_type;
  rtr->recv_ctx = reinterpret_cast<uintptr_t>(rdv_ctx);
  // int nrdmas = LCII_calculate_rdma_num(rdv_ctx);
  // if (nrdmas == 1 && LCI_RDV_PROTOCOL == LCI_RDV_WRITEIMM &&
  //     rdv_type != LCII_RDV_IOVEC) {
  //   // We cannot use writeimm for more than 1 rdma messages.
  //   // IOVEC does not support writeimm for now
  //   uint64_t ctx_key;
  //   LCII_PCOUNTER_START(rts_archive_timer);
  //   int result =
  //       LCM_archive_put(ep->ctx_archive_p, (uintptr_t)rdv_ctx, &ctx_key);
  //   LCII_PCOUNTER_END(rts_archive_timer);
  //   // TODO: be able to pass back pressure to user
  //   LCI_Assert(result == LCM_SUCCESS, "Archive is full!\n");
  //   packet->data.rtr.recv_ctx_key = ctx_key;
  // } else {
  // packet->data.rtr.recv_ctx = (uintptr_t)rdv_ctx;
  // }
  // if (rdv_ctx->data_type == LCI_LONG) {
  rtr->load_data(rdv_ctx->data);

  // log
  LCI_DBG_Log(LOG_TRACE, "rdv", "send rtr: sctx %p\n",
              (void*)packet->data.rtr.send_ctx);

  // send the rtr packet
  // endpoint_t endpoint_to_use;
  // if (is_in_progress) {
  //   endpoint_to_use = ep->device->endpoint_progress->endpoint;
  // } else {
  //   LCI_Assert(rdv_type == LCII_RDV_2SIDED, "");
  //   endpoint_to_use = ep->device->endpoint_worker->endpoint;
  // }
  internal_context_t* rtr_ctx = new internal_context_t;
  rtr_ctx->packet = packet;

  size_t length = rtr->get_size();
  net_imm_data_t imm_data = set_bits32(0, IMM_DATA_MSG_RTR, 2, 29);
  error_t status = net_endpoint.get_impl()->post_send(
      (int)rdv_ctx->rank, packet->payload, length, packet->get_mr(net_endpoint),
      imm_data, rtr_ctx);
  LCI_Assert(status.is_posted(), "Unexpected error value\n");
}

inline void handle_rdv_rtr(runtime_t runtime, net_endpoint_t net_endpoint,
                           packet_t* packet)
{
  // the sender side handles the rtr message
  net_device_t net_device = net_endpoint.get_impl()->net_device;
  net_context_t net_context = net_device.get_impl()->net_context;
  rtr_msg_t* rtr = reinterpret_cast<rtr_msg_t*>(packet->payload);
  rdv_type_t rdv_type = rtr->rdv_type;
  internal_context_t* ctx = (internal_context_t*)rtr->send_ctx;
  // Set up the "extended context" for write protocol
  void* ctx_to_pass = ctx;
  int nrdmas = rtr->count;
  if (nrdmas > 1 ||
      runtime.get_attr_rdv_protocol() == attr_rdv_protocol_t::write ||
      rdv_type == rdv_type_t::multiple) {
    internal_context_extended_t* ectx = new internal_context_extended_t;
    ectx->internal_ctx = ctx;
    ectx->signal_count = nrdmas;
    ectx->recv_ctx = rtr->recv_ctx;
    ctx_to_pass = ectx;
  }
  for (int i = 0; i < rtr->count; ++i) {
    void* buffer;
    size_t size;
    mr_t* p_mr;
    if (ctx->data.is_buffer()) {
      buffer = ctx->data.buffer.base;
      size = ctx->data.buffer.size;
      p_mr = &ctx->data.buffer.mr;
    } else {
      LCI_Assert(ctx->data.is_buffers(), "Unexpected data type\n");
      buffer = ctx->data.buffers.buffers[i].base;
      size = ctx->data.buffers.buffers[i].size;
      p_mr = &ctx->data.buffers.buffers[i].mr;
    }
    // register the buffer if necessary
    if (p_mr->is_empty()) {
      ctx->mr_on_the_fly = true;
      *p_mr = register_memory_x(buffer, size)
                  .runtime(runtime)
                  .net_device(net_device)();
    } else {
      ctx->mr_on_the_fly = false;
    }
    // issue the put/putimm
    // if (nrdmas > 1 || LCI_RDV_PROTOCOL == LCI_RDV_WRITE ||
    // rdv_type == LCII_RDV_IOVEC) {
    size_t max_single_msg_size = net_context.get_attr_max_msg_size();
    if (size > max_single_msg_size) {
      LCI_DBG_Log(LOG_TRACE, "rdv", "Splitting a large message of %lu bytes\n",
                  lbuffer->length);
    }
    for (size_t offset = 0; offset < size; offset += max_single_msg_size) {
      char* address = (char*)buffer + offset;
      size_t length = std::min(size - offset, max_single_msg_size);
      error_t error = net_endpoint.get_impl()->post_put(
          (int)ctx->rank, address, length, *p_mr,
          rtr->rbuffer_info_p[i].remote_addr_base,
          rtr->rbuffer_info_p[i].remote_addr_offset + offset,
          rtr->rbuffer_info_p[i].rkey, ctx_to_pass);
      LCI_Assert(error.is_posted(), "Unexpected error value\n");
    }
    // } else {
    // LCI_DBG_Assert(lbuffer->length <= LCI_MAX_SINGLE_MESSAGE_SIZE &&
    //  LCI_RDV_PROTOCOL == LCI_RDV_WRITEIMM &&
    //  rdv_type != LCII_RDV_IOVEC,
    //  "Unexpected rdv protocol!\n");
    // post_putImm_bq(ep->bq_p, ep->bq_spinlock_p,
    // ep->device->endpoint_progress->endpoint,
    // (int)ctx->rank, lbuffer->address, lbuffer->length,
    // lbuffer->segment->mr,
    // packet->data.rtr.rbuffer_info_p[0].remote_addr_base,
    // packet->data.rtr.rbuffer_info_p[0].remote_addr_offset,
    // packet->data.rtr.rbuffer_info_p[0].rkey,
    // LCII_MAKE_PROTO(ep->gid, LCI_MSG_RDV_DATA,
    // packet->data.rtr.recv_ctx_key),
    // ctx_to_pass);
    // }
  }
  // free the rtr packet
  packet->put_back();
}

inline void handle_rdv_local_write(net_endpoint_t net_endpoint,
                                   internal_context_extended_t* ectx)
{
  int signal_count = --ectx->signal_count;
  if (signal_count > 0) {
    return;
  }
  LCI_DBG_Assert(signal_count == 0, "Unexpected signal!\n");
  internal_context_t* ctx = ectx->internal_ctx;
  LCI_DBG_Log(LOG_TRACE, "rdv", "send FIN: rctx %p\n", (void*)ectx->recv_ctx);
  net_imm_data_t imm_data = set_bits32(0, IMM_DATA_MSG_FIN, 2, 29);
  error_t error = net_endpoint.get_impl()->post_sends(
      (int)ctx->rank, &ectx->recv_ctx, sizeof(ectx->recv_ctx), imm_data);
  LCI_Assert(error.is_ok(), "Unexpected error value\n");
  delete ectx;
  free_ctx_and_signal_comp(ctx);
}

inline void handle_rdv_remote_comp(internal_context_t* ctx)
{
  // We have to count data received by remote put here.
  // if (ctx->data_type == LCI_LONG) {
  //   LCII_PCOUNTER_ADD(net_recv_comp, ctx->data.lbuffer.length);
  // } else {
  //   for (int i = 0; i < ctx->data.iovec.count; ++i)
  //     LCII_PCOUNTER_ADD(net_recv_comp, ctx->data.iovec.lbuffers[i].length);
  // }
  free_ctx_and_signal_comp(ctx);
}

inline void handle_rdv_fin(packet_t* packet)
{
  internal_context_t* ctx;
  memcpy(&ctx, packet->payload, sizeof(ctx));
  LCI_DBG_Log(LOG_TRACE, "rdv", "recv FIN: rctx %p\n", ctx);
  packet->put_back();
  handle_rdv_remote_comp(ctx);
}

// inline void handle_rdv_remote_writeImm()
// {
//   LCII_context_t* ctx =
//       (LCII_context_t*)LCM_archive_remove(ep->ctx_archive_p, ctx_key);
//   LCI_DBG_Log(LOG_TRACE, "rdv",
//               "complete recvl: ctx %p rank %d "
//               "tag %d user_ctx %p completion attr %x completion %p\n",
//               ctx, ctx->rank, ctx->tag, ctx->user_context, ctx->comp_attr,
//               ctx->completion);
//   handle_rdv_remote_comp(ctx);
// }

}  // namespace lci

#endif  // LCI_CORE_RENDEZVOUS_HPP