#include "lcixx_internal.hpp"

namespace lcixx
{
void alloc_cq_x::call() const { comp_->p_impl = new cq_impl_t(); }

void free_cq_x::call() const
{
  cq_impl_t* p_cq = static_cast<cq_impl_t*>(comp_->p_impl);
  delete p_cq;
  comp_->p_impl = nullptr;
}

void cq_pop_x::call() const
{
  cq_impl_t* p_cq = static_cast<cq_impl_t*>(comp_.p_impl);
  bool succeed = p_cq->pop(status_);
  if (!succeed) {
    error_->reset(errorcode_t::retry);
  } else {
    error_->reset(errorcode_t::ok);
  }
}

}  // namespace lcixx