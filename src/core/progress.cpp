#include "lci_internal.hpp"

namespace lci
{
void progress_recv(net_device_t net_device, const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_recv_comp, 1)
  packet_t* packet = static_cast<packet_t*>(net_status.ctx);
  uint32_t imm_data = net_status.imm_data;
  tag_t tag = get_bits32(imm_data, 16, 0);
  rcomp_t remote_comp = get_bits32(imm_data, 15, 16);
  bool is_eager = get_bits32(imm_data, 1, 31);
  if (is_eager) {
    comp_t comp = g_default_runtime.p_impl->rcomp_registry.get(remote_comp);
    status_t status;
    status.error = errorcode_t::ok;
    status.rank = net_status.rank;
    status.tag = tag;
    status.buffer = malloc(net_status.length);
    memcpy(status.buffer, packet->get_message_address(), net_status.length);
    status.size = net_status.length;
    status.ctx = nullptr;
    packet->put_back();
    comp.p_impl->signal(status);
  } else {
    // rts, rtr, or fin
    throw std::logic_error("Not implemented");
  }
}

void progress_send(const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_send_comp, 1)
  internal_context_t* internal_ctx =
      static_cast<internal_context_t*>(net_status.ctx);
  if (internal_ctx->packet) {
    internal_ctx->packet->put_back();
  }
  if (internal_ctx->comp.p_impl) {
    status_t status = internal_ctx->get_status();
    comp_t comp = internal_ctx->comp;
    delete internal_ctx;
    comp.p_impl->signal(status);
  } else {
    throw std::runtime_error("Should not happen");
  }
}

void progress_write(const net_status_t& status)
{
  throw std::runtime_error("progress_write not implemented");
}

void progress_remote_write(const net_status_t& status)
{
  throw std::runtime_error("progress_remote_write not implemented");
}

void progress_read(const net_status_t& status)
{
  throw std::runtime_error("progress_read not implemented");
}

error_t progress_x::call_impl(runtime_t runtime, net_device_t net_device) const
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
      progress_recv(net_device, status);
    } else if (status.opcode == net_opcode_t::SEND) {
      progress_send(status);
    } else if (status.opcode == net_opcode_t::WRITE) {
      progress_write(status);
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