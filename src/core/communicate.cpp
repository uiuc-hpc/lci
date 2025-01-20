#include "lcixx_internal.hpp"

namespace lcixx
{
void communicate_x::call() const
{
  runtime_t runtime = runtime_.get_value_or(g_default_runtime);
  net_endpoint_t net_endpoint;
  if (!net_endpoint_.get_value(&net_endpoint)) {
    get_default_net_endpoint_x(&net_endpoint).runtime(runtime).call();
  }
  void* ctx = ctx_.get_value_or(nullptr);
  net_imm_data_t imm_data = imm_data_.get_value_or(0);
  // get a packet
  // allocate internal status object
  // post send
  *error_ =
      net_endpoint.p_impl->post_send(rank_, buffer_, size_, mr_, imm_data, ctx);
}
}  // namespace lcixx