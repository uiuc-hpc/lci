#ifndef LCI_CORE_RENDEZVOUS_HPP
#define LCI_CORE_RENDEZVOUS_HPP

namespace lci
{
inline void handle_rdv_rts_post(runtime_t runtime, net_endpoint_t net_endpoint,
                                packet_t* packet, internal_context_t* rdv_ctx);
// Also for eager protocol
inline void handle_matched_sendrecv(runtime_t runtime,
                                    net_endpoint_t net_endpoint,
                                    packet_t* packet,
                                    internal_context_t* recv_ctx,
                                    status_t* p_status = nullptr)
{
  if (packet->local_context.is_eager) {
    // information needed from the incoming send packet
    // rank, tag, actual size, packet buffer, eager or rendezvous
    // information needed from the posted recv
    // user buffer, user_context, comp
    comp_t comp = recv_ctx->comp;

    status_t status;
    status.error = errorcode_t::ok;
    status.data = recv_ctx->data;
    status.user_context = recv_ctx->user_context;
    delete recv_ctx;
    status.rank = packet->local_context.rank;
    status.tag = packet->local_context.tag;
    LCI_Assert(status.data.is_buffer(), "Unexpected data type\n");
    std::memcpy(status.data.buffer.base, packet->get_payload_address(),
                packet->local_context.data.buffer.size);
    status.data.buffer.size = packet->local_context.data.buffer.size;
    packet->put_back();
    if (!p_status) {
      comp.get_impl()->signal(status);
    } else {
      *p_status = status;
    }
  } else {
    handle_rdv_rts_post(runtime, net_endpoint, packet, recv_ctx);
  }
}

struct rts_msg_t {
  rdv_type_t rdv_type : 2;
  int count : 30;
  rcomp_t rcomp;
  uintptr_t send_ctx;
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
      rdv_type = rdv_type_t::single;
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

inline void handle_rdv_rts_post(runtime_t runtime, net_endpoint_t net_endpoint,
                                packet_t* packet, internal_context_t* rdv_ctx);

inline void handle_rdv_rts(runtime_t runtime, net_endpoint_t net_endpoint,
                           packet_t* packet)
{
  net_device_t net_device = net_endpoint.get_impl()->net_device;
  // Extract information from the received RTS packet
  rts_msg_t* rts = reinterpret_cast<rts_msg_t*>(packet->payload);
  rdv_type_t rdv_type = rts->rdv_type;
  LCI_DBG_Log(LOG_TRACE, "rdv", "handle rts: rdv_type %d\n", rdv_type);

  auto entry = runtime.p_impl->rhandler_registry.get(rts->rcomp);
  if (entry.type == rhandler_registry_t::type_t::matching_engine) {
    // send recv
    packet->local_context.tag = rts->tag;
    packet->local_context.is_eager = false;

    // get the matching engine
    matching_engine_impl_t* p_matching_engine =
        reinterpret_cast<matching_engine_impl_t*>(entry.value);
    // insert into the matching engine
    auto key = p_matching_engine->make_key(packet->local_context.rank,
                                           packet->local_context.tag);
    auto ret = p_matching_engine->insert(key, packet,
                                         matching_engine_impl_t::type_t::send);
    if (!ret) return;
    handle_rdv_rts_post(runtime, net_endpoint, packet,
                        reinterpret_cast<internal_context_t*>(ret));
  } else {
    // am
    handle_rdv_rts_post(runtime, net_endpoint, packet, nullptr);
  }
}

inline void handle_rdv_rts_post(runtime_t runtime, net_endpoint_t net_endpoint,
                                packet_t* packet, internal_context_t* rdv_ctx)
{
  net_device_t net_device = net_endpoint.get_impl()->net_device;
  // Extract information from the received RTS packet
  rts_msg_t* rts = reinterpret_cast<rts_msg_t*>(packet->payload);
  rdv_type_t rdv_type = rts->rdv_type;
  LCI_DBG_Log(LOG_TRACE, "rdv", "handle rts: rdv_type %d\n", rdv_type);
  // build the rdv context
  if (!rdv_ctx) {
    rdv_ctx = new internal_context_t;
    rdv_ctx->data = rts->alloc_data();
    rdv_ctx->user_context = NULL;
    rdv_ctx->rdv_type = rdv_type;
    auto entry = runtime.p_impl->rhandler_registry.get(rts->rcomp);
    LCI_DBG_Assert(entry.type == rhandler_registry_t::type_t::comp, "");
    rdv_ctx->comp.p_impl = reinterpret_cast<comp_impl_t*>(entry.value);
  } else {
    // needed from incoming send
    // packet, src_rank, tag
    // needed from posted recv
    // user_data, user_context, comp

    // set rdv_ctx->data size(s) based on rts->size_p
    if (rdv_ctx->data.is_buffer()) {
      LCI_Assert(rts->count == 1, "");
      LCI_Assert(rdv_ctx->data.buffer.size >= rts->size_p[0], "");
      rdv_ctx->data.buffer.size = rts->size_p[0];
    } else {
      LCI_Assert(rdv_ctx->data.is_buffers(), "");
      LCI_Assert(rdv_ctx->data.get_buffers_count() == rts->count, "");
      for (int i = 0; i < rts->count; i++) {
        LCI_Assert(rdv_ctx->data.buffers.buffers[i].size >= rts->size_p[i], "");
        rdv_ctx->data.buffers.buffers[i].size = rts->size_p[i];
      }
    }
  }
  rdv_ctx->tag = rts->tag;
  rdv_ctx->rank = packet->local_context.rank;

  // Register the data
  rdv_ctx->mr_on_the_fly = register_data(rdv_ctx->data, net_device);

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