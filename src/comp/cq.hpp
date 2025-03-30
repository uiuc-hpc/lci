// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_DATA_STRUCTURE_CQ_CQ_HPP
#define LCI_DATA_STRUCTURE_CQ_CQ_HPP

namespace lci
{
class cq_t : public comp_impl_t
{
 public:
  cq_t(comp_attr_t attr_, int default_length_)
      : comp_impl_t(attr_), default_length(default_length_)
  {
    attr.comp_type = attr_comp_type_t::cq;
    LCT_queue_type_t cq_type = LCT_QUEUE_ARRAY_ATOMIC_FAA;
    switch (attr.cq_type) {
      case attr_cq_type_t::array_atomic:
        cq_type = LCT_QUEUE_ARRAY_ATOMIC_FAA;
        break;
      case attr_cq_type_t::lcrq:
        cq_type = LCT_QUEUE_LCRQ;
        break;
      default:
        LCI_Assert(false, "cq type is not supported!\n");
    }
    queue = LCT_queue_alloc(cq_type, default_length);
  }
  ~cq_t() { LCT_queue_free(&queue); }
  void signal(status_t status) override
  {
    LCI_Assert(status.error.is_ok(), "status.error is not ok!\n");
    LCI_PCOUNTER_ADD(comp_produce, 1);
    status_t* p = new status_t(std::move(status));
    LCT_queue_push(queue, p);
  }
  status_t pop()
  {
    // TODO: let cq directly store status_t instead of a pointer
    status_t status;
    status_t* p = static_cast<status_t*>(LCT_queue_pop(queue));
    if (p == nullptr) {
      LCI_Assert(status.error.is_retry(), "status.error is not retry!\n");
      return status;
    } else {
      status = std::move(*p);
      delete p;
      LCI_PCOUNTER_ADD(comp_consume, 1);
      LCI_Assert(status.error.is_ok(), "status.error is not ok!\n");
      return status;
    }
  }

 private:
  size_t default_length;
  LCT_queue_t queue;
};

inline status_t cq_pop_x::call_impl(comp_t comp, runtime_t) const
{
  cq_t* p_cq = static_cast<cq_t*>(comp.p_impl);
  return p_cq->pop();
}

}  // namespace lci

#endif  // LCI_DATA_STRUCTURE_CQ_CQ_HPP