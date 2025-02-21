#ifndef LCI_CORE_RENDEZVOUS_HPP
#define LCI_CORE_RENDEZVOUS_HPP

namespace lci
{
struct __attribute__((packed)) rts_msg_t {
  uintptr_t
      send_ctx; /* the address of the related context on the source side */
  rdv_type_t rdv_type; /* type of this rendezvous message */
  tag_t tag;
  rcomp_t rcomp;
  union {
    // for a single message
    size_t size;
    // for iovec
    struct {
      int count;
      size_t piggy_back_size;
      size_t size_p[0];
    };
  };

  static constexpr size_t get_size_plain()
  {
    return offsetof(rts_msg_t, size) + sizeof(size);
  }

  static size_t get_size_iovec(int count)
  {
    return offsetof(rts_msg_t, size_p) + count * sizeof(size_t);
  }
};

struct __attribute__((packed)) rtr_rbuffer_info_t {
  rkey_t rkey;
  uintptr_t remote_addr_base;
  uint64_t remote_addr_offset;
};

struct __attribute__((packed)) rtr_msg_t {
  uintptr_t
      send_ctx; /* the address of the related context on the source side */
  rdv_type_t rdv_type; /* type of this rendezvous message */
  union {
    // When using writeimm protocol
    uint32_t
        recv_ctx_key; /* the id of the related context on the target side */
    // when using write protocol
    uintptr_t recv_ctx;
  };
  rtr_rbuffer_info_t rbuffer_info_p[0];

  static constexpr size_t get_size_single()
  {
    return offsetof(rtr_msg_t, rbuffer_info_p);
  }

  static size_t get_size_iovec(int count)
  {
    return offsetof(rtr_msg_t, rbuffer_info_p) +
           count * sizeof(rtr_rbuffer_info_t);
  }
};

inline void fill_rtr_rbuffer_info(rtr_rbuffer_info_t* p, void* buffer, mr_t mr)
{
  p->remote_addr_base = (uintptr_t)mr.get_impl()->address;
  p->remote_addr_offset = (uintptr_t)buffer - p->remote_addr_base;
  p->rkey = get_rkey(mr);
}

inline void handle_rdv_rts(runtime_t runtime, net_endpoint_t net_endpoint,
                           packet_t* packet, int src_rank,
                           internal_context_t* rdv_ctx, bool is_in_progress)
{
  // Extract information from the received RTS packet
  rts_msg_t* rts = reinterpret_cast<rts_msg_t*>(packet->fast.data.address);
  rdv_type_t rdv_type = rts->rdv_type;
  LCI_DBG_Log(LOG_TRACE, "rdv", "handle rts: rdv_type %d\n", rdv_type);
  if (!rdv_ctx) {
    LCI_Assert(
        rdv_type == rdv_type_t::single_1sided || rdv_type == rdv_type_t::iovec,
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
  rdv_ctx->size = rts->size;
  rdv_ctx->rank = src_rank;
  if (rdv_type == rdv_type_t::single_1sided || rdv_type == rdv_type_t::iovec) {
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
  rdv_ctx->buffer = alloc_memalign(LCI_CACHE_LINE, rdv_ctx->size);
  rdv_ctx->mr = register_memory_x(rdv_ctx->buffer, rdv_ctx->size)
                    .net_device(net_endpoint.p_impl->net_device)();
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
  rtr_msg_t* rtr = reinterpret_cast<rtr_msg_t*>(packet->fast.data.address);
  packet->local_context.local_id = mpmc_set_t::LOCAL_SET_ID_NULL;
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
  fill_rtr_rbuffer_info(&rtr->rbuffer_info_p[0], rdv_ctx->buffer, rdv_ctx->mr);
  // } else {
  //   LCI_DBG_Assert(rdv_ctx->data_type == LCI_IOVEC, "");
  //   for (int i = 0; i < rdv_ctx->data.iovec.count; ++i) {
  //     LCII_rts_fill_rbuffer_info(&packet->data.rtr.rbuffer_info_p[i],
  //                                rdv_ctx->data.iovec.lbuffers[i]);
  //   }
  // }

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

  size_t num_rbuffer_info = 1;
  // (rdv_ctx->data_type == LCI_IOVEC) ? rdv_ctx->data.iovec.count : 1;
  size_t length =
      (uintptr_t)&rtr->rbuffer_info_p[num_rbuffer_info] - (uintptr_t)rtr;
  net_imm_data_t imm_data = set_bits32(imm_data, IMM_DATA_MSG_RTR, 2, 29);
  net_endpoint.get_impl()->post_send(
      (int)rdv_ctx->rank, packet->fast.data.address, length,
      packet->get_mr(net_endpoint), imm_data, rtr_ctx);
}

inline void handle_rdv_rtr(runtime_t runtime, net_endpoint_t net_endpoint,
                           packet_t* packet)
{
  // the sender side handles the rtr message
  net_device_t net_device = net_endpoint.get_impl()->net_device;
  net_context_t net_context = net_device.get_impl()->net_context;
  rtr_msg_t* rtr = reinterpret_cast<rtr_msg_t*>(packet->fast.data.address);
  rdv_type_t rdv_type = rtr->rdv_type;
  internal_context_t* ctx = (internal_context_t*)rtr->send_ctx;
  // Set up the "extended context" for write protocol
  void* ctx_to_pass = ctx;
  // int nrdmas = LCII_calculate_rdma_num(ctx);
  int nrdmas = 1;
  if (nrdmas > 1 ||
      runtime.get_attr_rdv_protocol() == attr_rdv_protocol_t::write ||
      rdv_type == rdv_type_t::iovec) {
    internal_context_extended_t* ectx = new internal_context_extended_t;
    ectx->internal_ctx = ctx;
    ectx->signal_count = nrdmas;
    ectx->recv_ctx = rtr->recv_ctx;
    ctx_to_pass = ectx;
  }
  // int niters = (ctx->data_type == LCI_IOVEC) ? ctx->data.iovec.count : 1;
  int niters = 1;
  for (int i = 0; i < niters; ++i) {
    void* buffer = ctx->buffer;
    size_t size = ctx->size;
    // LCI_lbuffer_t* lbuffer;
    // if (ctx->data_type == LCI_LONG) {
    // lbuffer = &ctx->data.lbuffer;
    // } else {
    // LCI_DBG_Assert(ctx->data_type == LCI_IOVEC, "");
    // lbuffer = &ctx->data.iovec.lbuffers[i];
    // }
    // register the buffer if necessary
    if (ctx->mr.is_empty()) {
      ctx->mr_on_the_fly = true;
      ctx->mr = register_memory_x(buffer, size).net_device(net_device)();
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
      net_endpoint.get_impl()->post_put(
          (int)ctx->rank, address, length, ctx->mr,
          rtr->rbuffer_info_p[i].remote_addr_base,
          rtr->rbuffer_info_p[i].remote_addr_offset + offset,
          rtr->rbuffer_info_p[i].rkey, ctx_to_pass);
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
  net_imm_data_t imm_data = set_bits32(imm_data, IMM_DATA_MSG_FIN, 2, 29);
  net_endpoint.get_impl()->post_sends((int)ctx->rank, &ectx->recv_ctx,
                                      sizeof(ectx->recv_ctx), imm_data);
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
  memcpy(&ctx, packet->fast.data.address, sizeof(ctx));
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