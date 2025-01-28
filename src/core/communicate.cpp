#include "lcixx_internal.hpp"

namespace lcixx
{
void communicate_x::call() const
{
  // positional arguments
  direction_t direction = direction_;
  int rank = rank_;
  void* local_buffer = local_buffer_;
  size_t size = size_;
  comp_t local_comp = local_comp_;
  // get a packet
  // optional arguments
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_endpoint_t net_endpoint;
  if (!net_endpoint_.get_value(&net_endpoint)) {
    get_default_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  }
  net_device_t net_device = net_endpoint.p_impl->net_device;
  packet_pool_t packet_pool;
  if (!packet_pool_.get_value(&packet_pool)) {
    get_default_packet_pool_x(&packet_pool).runtime(runtime).call();
  }
  void* ctx = ctx_.get_value_or(nullptr);
  tag_t tag = tag_.get_value_or(0);
  rcomp_t remote_comp = remote_comp_.get_value_or(0);

  // allocate internal status object
  internal_context_t* internal_ctx = new internal_context_t;
  internal_ctx->rank = rank;
  internal_ctx->tag = tag;
  internal_ctx->buffer = local_buffer;
  internal_ctx->size = size;
  internal_ctx->comp = local_comp;
  internal_ctx->user_context = ctx;

  packet_t* packet;
  if (direction == direction_t::SEND) {
    // get a packet
    if (packet_pool.p_impl->is_packet(local_buffer)) {
      // users provide a packet
      packet = address2packet(local_buffer);
      internal_ctx->buffer = nullptr;  // users do not own the packet anymore
    } else {
      // allocate a packet
      packet = packet_pool.p_impl->get();
      if (!packet) {
        error_->reset(errorcode_t::retry_nomem);
        goto exit_free_ctx;
      }
      memcpy(packet->get_message_address(), local_buffer, size);
    }
    packet->local_context.local_id =
        (size > runtime.get_attr_packet_return_threshold())
            ? packet_pool.p_impl->pool.get_local_set_id()
            : mpmc_set_t::LOCAL_SET_ID_NULL;
    internal_ctx->packet = packet;
    bool is_eager = true;

    // set immediate data
    // is_eager (1) ; remote_comp (15) ; tag (16)
    uint32_t imm_data = set_bits32(imm_data, tag, 16, 0);
    imm_data = set_bits32(imm_data, remote_comp, 15, 16);
    imm_data = set_bits32(imm_data, is_eager, 1, 31);
    // post send
    *error_ = net_endpoint.p_impl->post_send(
        rank, packet->get_message_address(), size, packet->get_mr(net_device),
        imm_data, internal_ctx);
    if (error_->is_retry()) {
      goto exit_free_packet;
    } else {
      goto exit;
    }
  } else {
    throw std::logic_error("Not implemented");
  }

exit_free_packet:
  put_packet_x(packet).runtime(runtime).call();

exit_free_ctx:
  delete internal_ctx;

exit:
  return;
}
}  // namespace lcixx