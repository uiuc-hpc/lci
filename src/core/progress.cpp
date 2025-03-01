#include "lci_internal.hpp"

namespace lci
{
void progress_recv(runtime_t runtime, net_device_t net_device,
                   net_endpoint_t net_endpoint, const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_recv_comp, 1)
  packet_t* packet = static_cast<packet_t*>(net_status.user_context);
  // decode immediate data
  uint32_t imm_data = net_status.imm_data;
  tag_t tag;
  rcomp_t remote_comp;
  bool is_fastpath = get_bits32(imm_data, 1, 31);
  imm_data_msg_type_t msg_type;
  if (is_fastpath) {
    tag = get_bits32(imm_data, 16, 0);
    remote_comp = get_bits32(imm_data, 15, 16);
    msg_type = IMM_DATA_MSG_EAGER;
  } else {
    msg_type = static_cast<imm_data_msg_type_t>(get_bits32(imm_data, 2, 29));
    if (msg_type == IMM_DATA_MSG_EAGER) {
      // get tag and rcomp by looking at the message payload
      throw std::logic_error("Not implemented");
    }
  }
  switch (msg_type) {
    case IMM_DATA_MSG_EAGER: {
      auto entry = runtime.p_impl->rhandler_registry.get(remote_comp);
      if (entry.type == rhandler_registry_t::type_t::comp) {
        status_t status;
        status.error = errorcode_t::ok;
        status.rank = net_status.rank;
        status.tag = tag;
        status.data.copy_from(packet->get_payload_address(), net_status.length);
        status.user_context = nullptr;
        packet->put_back();
        reinterpret_cast<comp_impl_t*>(entry.value)->signal(std::move(status));
      } else {
        // we get a matching table entry
        matching_engine_impl_t* p_matching_engine =
            reinterpret_cast<matching_engine_impl_t*>(entry.value);
        auto key = p_matching_engine->make_key(net_status.rank, tag);
        packet->local_context.is_eager = true;
        packet->local_context.rank = net_status.rank;
        packet->local_context.tag = tag;
        packet->local_context.data = buffer_t(nullptr, net_status.length);
        auto ret = p_matching_engine->insert(
            key, packet, matching_engine_impl_t::type_t::send);
        if (ret)
          handle_matched_sendrecv(runtime, net_endpoint, packet,
                                  reinterpret_cast<internal_context_t*>(ret));
      }
      break;
    }
    case IMM_DATA_MSG_RTS:
      packet->local_context.rank = net_status.rank;
      handle_rdv_rts(runtime, net_endpoint, packet);
      break;
    case IMM_DATA_MSG_RTR:
      handle_rdv_rtr(runtime, net_endpoint, packet);
      break;
    case IMM_DATA_MSG_FIN:
      handle_rdv_fin(packet);
      break;
    default:
      throw std::logic_error("Not implemented");
  }
}

void progress_send(const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_send_comp, 1)
  internal_context_t* internal_ctx =
      static_cast<internal_context_t*>(net_status.user_context);
  if (!internal_ctx)
    // an ibv inject
    return;
  free_ctx_and_signal_comp(internal_ctx);
}

void progress_write(net_endpoint_t net_endpoint, const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_write_comp, 1)
  internal_context_t* internal_ctx =
      static_cast<internal_context_t*>(net_status.user_context);

  if (internal_ctx->is_extended) {
    // extended internal context
    internal_context_extended_t* ectx =
        reinterpret_cast<internal_context_extended_t*>(internal_ctx);
    int signal_count = --ectx->signal_count;
    if (signal_count > 0) {
      return;
    }
    LCI_DBG_Assert(signal_count == 0, "Unexpected signal!\n");
    internal_context_t* ctx = ectx->internal_ctx;
    if (ectx->recv_ctx) {
      handle_rdv_local_write(net_endpoint, ectx);
    }  // else: this is a RDMA write buffers
    delete ectx;
    free_ctx_and_signal_comp(ctx);
  } else {
    free_ctx_and_signal_comp(internal_ctx);
  }
}

void progress_remote_write(const net_status_t& status)
{
  throw std::runtime_error("progress_remote_write not implemented");
}

void progress_read(const net_status_t& status)
{
  throw std::runtime_error("progress_read not implemented");
}

error_t progress_x::call_impl(runtime_t runtime, net_device_t net_device,
                              net_endpoint_t net_endpoint) const
{
  LCI_PCOUNTER_ADD(progress, 1);
  error_t error(errorcode_t::retry);
  std::vector<net_status_t> statuses = net_poll_cq_x()
                                           .net_device(net_device)
                                           .runtime(runtime)
                                           .max_polls(20)
                                           .call();
  if (!statuses.empty()) {
    error.reset(errorcode_t::ok);
  }
  for (auto& status : statuses) {
    if (status.opcode == net_opcode_t::RECV) {
      net_device.p_impl->consume_recvs(1);
      progress_recv(runtime, net_device, net_endpoint, status);
    } else if (status.opcode == net_opcode_t::SEND) {
      progress_send(status);
    } else if (status.opcode == net_opcode_t::WRITE) {
      progress_write(net_endpoint, status);
    } else if (status.opcode == net_opcode_t::REMOTE_WRITE) {
      progress_remote_write(status);
    } else if (status.opcode == net_opcode_t::READ) {
      progress_read(status);
    }
  }
  net_device.p_impl->refill_recvs();
  return error;
}

}  // namespace lci