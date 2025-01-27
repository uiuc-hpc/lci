#include "lcixx_internal.hpp"

namespace lcixx
{
void progress_recv(const net_status_t& net_status)
{
  packet_t* packet = static_cast<packet_t*>(net_status.ctx);
  uint32_t imm_data = net_status.imm_data;
  tag_t tag = get_bits32(imm_data, 16, 0);
  rcomp_t remote_comp = get_bits32(imm_data, 15, 16);
  bool is_eager = get_bits32(imm_data, 1, 31);
  if (is_eager) {
    rhandler_t rhandler = g_default_runtime.rhandler_table.get(
        remote_comp) if (rhandler.is_comp())
    {
      comp_t comp = rhandler.get_comp();
      status_t status;
      status.error = errorcode_t::ok;
      status.rank = net_status.rank;
      status.tag = tag;
      status.buffer = malloc(net_status.length);
      memcpy(status.buffer, packet->get_message_address(), net_status.length);
      status.size = net_status.length;
      status.ctx = nullptr;
      internal_ctx->comp.p_impl->signal(status);
    }
    else
    {
      // matching table
      throw std::logic_error("Not implemented");
    }
  } else {
    // rts, rtr, or fin
    throw std::logic_error("Not implemented");
  }
  put_packet_x(packet).call();
}

void progress_send(const net_status_t& net_status)
{
  internal_context_t* internal_ctx =
      static_cast<internal_context_t*>(net_status.ctx);
  if (internal_ctx->packet) {
    put_packet_x(internal_ctx->packet).call();
  }
  if (internal_ctx->comp.p_impl) {
    status_t status = internal_ctx->get_status();
    internal_ctx->comp.p_impl->signal(status);
  }
  delete internal_ctx;
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

void progress_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_device_t net_device;
  if (!net_device_.get_value(&net_device)) {
    get_default_net_device_x(&net_device).runtime(runtime).call();
  }

  std::vector<net_status_t> statuses;
  net_poll_cq_x(&statuses)
      .net_device(net_device)
      .runtime(runtime)
      .max_polls(20)
      .call();
  for (auto& status : statuses) {
    if (status.opcode == net_opcode_t::RECV) {
      progress_recv(status);
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
  return;
}

}  // namespace lcixx