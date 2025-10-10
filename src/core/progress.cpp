// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace lci
{
void progress_recv(runtime_t runtime, endpoint_t endpoint,
                   const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_recv_comp, 1)
  packet_t* packet = static_cast<packet_t*>(net_status.user_context);
  size_t msg_size = net_status.length;
  // decode immediate data
  uint32_t imm_data = net_status.imm_data;
  tag_t tag = 0;
  rcomp_t remote_comp = 0;
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
      msg_size -= sizeof(remote_comp);
      memcpy(&remote_comp, (char*)packet->get_payload_address() + msg_size,
             sizeof(remote_comp));
      msg_size -= sizeof(tag);
      memcpy(&tag, (char*)packet->get_payload_address() + msg_size,
             sizeof(tag));
    }
  }
  switch (msg_type) {
    case IMM_DATA_MSG_EAGER: {
      auto entry = runtime.p_impl->default_rhandler_registry.get(remote_comp);
      if (entry.type == rhandler_registry_t::type_t::comp) {
        // we get an active message
        status_t status;
        status.set_done();
        status.rank = net_status.rank;
        status.tag = tag;
        if (reinterpret_cast<comp_impl_t*>(entry.value)->attr.zero_copy_am) {
          status.buffer = packet->get_payload_address();
          status.size = msg_size;
        } else {
          // copy the data
          status.buffer = runtime.get_impl()->allocator->allocate(msg_size);
          status.size = msg_size;
          memcpy(status.buffer, packet->get_payload_address(), msg_size);
          packet->put_back();
        }
        status.user_context = nullptr;
        reinterpret_cast<comp_impl_t*>(entry.value)->signal(std::move(status));
      } else {
        // we get a matching table entry
        matching_engine_impl_t* p_matching_engine =
            reinterpret_cast<matching_engine_impl_t*>(entry.value);
        auto key = p_matching_engine->make_key(
            net_status.rank, tag,
            static_cast<matching_policy_t>(entry.metadata));
        packet->local_context.is_eager = true;
        packet->local_context.rank = net_status.rank;
        packet->local_context.tag = tag;
        packet->local_context.size = msg_size;
        auto ret = p_matching_engine->insert(
            key, packet, matching_engine_impl_t::insert_type_t::send);
        if (ret)
          handle_matched_sendrecv(runtime, endpoint, packet,
                                  reinterpret_cast<internal_context_t*>(ret));
      }
      break;
    }
    case IMM_DATA_MSG_RTS:
      packet->local_context.rank = net_status.rank;
      handle_rdv_rts(runtime, endpoint, packet);
      break;
    case IMM_DATA_MSG_RTR:
      handle_rdv_rtr(runtime, endpoint, packet);
      break;
    case IMM_DATA_MSG_FIN:
      handle_rdv_fin(packet);
      break;
    default:
      LCI_Assert(false, "Unknown message type %d\n", msg_type);
  }
}

void progress_send(const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_send_comp, 1)
  internal_context_t* internal_ctx =
      static_cast<internal_context_t*>(net_status.user_context);
  if (!internal_ctx) return;
  free_ctx_and_signal_comp(internal_ctx);
}

void progress_write(endpoint_t endpoint, const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_write_writeImm_comp, 1)
  internal_context_t* internal_ctx =
      static_cast<internal_context_t*>(net_status.user_context);

  if (!internal_ctx) return;

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
      handle_rdv_local_write(endpoint, ectx);
    } else if (ectx->imm_data_rank != -1) {
      // send immediate data
      error_t error = endpoint.get_impl()->post_sends(
          ectx->imm_data_rank, nullptr, 0, ectx->imm_data, nullptr,
          false /* allow_retry */);
      LCI_Assert(error.is_done(), "Unexpected error %d\n", error);
    }  // else: this is a RDMA write buffers
    delete ectx;
    free_ctx_and_signal_comp(ctx);
  } else {
    free_ctx_and_signal_comp(internal_ctx);
  }
}

void progress_remote_write(runtime_t runtime, const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_remote_write_comp, 1)
  packet_t* packet = static_cast<packet_t*>(net_status.user_context);
  if (packet) {
    packet->put_back();
  }
  // decode immediate data
  uint32_t imm_data = net_status.imm_data;
  tag_t tag;
  rcomp_t remote_comp;
  bool is_fastpath = get_bits32(imm_data, 1, 31);
  if (is_fastpath) {
    // user posted RDMA write with immediate data
    tag = get_bits32(imm_data, 16, 0);
    remote_comp = get_bits32(imm_data, 15, 16);
    auto entry = runtime.get_impl()->default_rhandler_registry.get(remote_comp);
    status_t status;
    status.set_done();
    status.rank = net_status.rank;
    status.tag = tag;
    status.user_context = nullptr;
    reinterpret_cast<comp_impl_t*>(entry.value)->signal(std::move(status));
  } else {
    LCI_Assert(false, "Not implemented");
  }
}

void progress_read(const net_status_t& net_status)
{
  LCI_PCOUNTER_ADD(net_read_comp, 1)
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
    delete ectx;
    free_ctx_and_signal_comp(ctx);
  } else {
    if (internal_ctx->packet_to_free) {
      memcpy(internal_ctx->buffer,
             internal_ctx->packet_to_free->get_payload_address(),
             internal_ctx->size);
    }
    free_ctx_and_signal_comp(internal_ctx);
  }
}

// for logging purposes
[[maybe_unused]] const uint64_t PROGRESS_LOG_INTERVAL = 1;  // 1s
thread_local uint64_t tls_update_counter = 0;
thread_local LCT_time_t tls_last_update_time = 0;
thread_local std::vector<uint64_t> tls_device_progress_counter_map(128, 0);

error_t progress_x::call_impl(runtime_t runtime, device_t device,
                              endpoint_t endpoint) const
{
  LCI_PCOUNTER_ADD(progress, 1);
  error_t error(errorcode_t::retry);

  // for (auto& endpoint : device.p_impl->endpoints) {
  for (int i = 0;
       i < device.get_impl()->next_endpoint_idx.load(std::memory_order_relaxed);
       i++) {
    endpoint_t endpoint = device.get_impl()->endpoints.get(i);
    if (endpoint.is_empty()) continue;
    // keep progressing the backlog queue until it is empty
    while (endpoint.get_impl()->progress_backlog_queue())
      error = errorcode_t::done;
  }
  // poll device completion queue
  net_status_t statuses[LCI_BACKEND_MAX_POLLS];
  size_t ret = device.get_impl()->poll_comp(statuses, LCI_BACKEND_MAX_POLLS);
  if (ret > 0) {
    error = errorcode_t::done;
    for (size_t i = 0; i < ret; i++) {
      auto status = statuses[i];
      if (status.opcode == net_opcode_t::RECV) {
        device.p_impl->consume_recvs(1);
        progress_recv(runtime, endpoint, status);
      } else if (status.opcode == net_opcode_t::SEND) {
        progress_send(status);
      } else if (status.opcode == net_opcode_t::WRITE) {
        progress_write(endpoint, status);
      } else if (status.opcode == net_opcode_t::REMOTE_WRITE) {
        progress_remote_write(runtime, status);
      } else if (status.opcode == net_opcode_t::READ) {
        progress_read(status);
      }
    }
  }
  if (device.p_impl->refill_recvs()) {
    error = errorcode_t::done;
  }

  // Log progress every 1s
#ifdef LCI_DEBUG
  int device_id = device.get_attr_uid();
  if (tls_device_progress_counter_map.size() <=
      static_cast<size_t>(device_id)) {
    tls_device_progress_counter_map.resize(device_id * 2 + 1, 0);
  }
  tls_device_progress_counter_map[device_id]++;
  if (++tls_update_counter % 1000000 == 0) {
    // check the timer every 1M progress
    LCT_time_t now = LCT_now();
    if (LCT_time_to_s(now - tls_last_update_time) > PROGRESS_LOG_INTERVAL) {
      tls_last_update_time = now;
      // print all counters
      std::string log_str =
          "Thread " + std::to_string(LCT_get_thread_id()) + " progressed ";
      for (size_t i = 0; i < tls_device_progress_counter_map.size(); i++) {
        if (tls_device_progress_counter_map[i] > 0) {
          log_str += std::to_string(tls_device_progress_counter_map[i]) +
                     " on device " + std::to_string(i);
        }
      }
      LCI_DBG_Log(LOG_TRACE, "progress", "%s\n", log_str.c_str());
    }
  }
#endif
  return error;
}

error_t test_drained_x::call_impl(runtime_t, device_t device) const
{
  // Relaxed memory order is sufficient here because we are not trying to ensure
  // any mutual exclusion
  for (int i = 0;
       i < device.get_impl()->next_endpoint_idx.load(std::memory_order_relaxed);
       i++) {
    endpoint_t endpoint = device.get_impl()->endpoints.get(i);
    if (endpoint.is_empty()) continue;
    if (endpoint.get_impl()->is_backlog_queue_empty() &&
        endpoint.get_impl()->get_pending_ops() == 0) {
      continue;
    } else {
      return errorcode_t::retry;
    }
  }
  return errorcode_t::done;
}

void wait_drained_x::call_impl(runtime_t, device_t device) const
{
  LCI_Log(LOG_TRACE, "network", "Enter wait_drained\n");
  while (test_drained_x().device(device)().is_retry()) {
    progress_x().device(device)();
  }
  LCI_Log(LOG_TRACE, "network", "Leave wait_drained\n");
}

}  // namespace lci
