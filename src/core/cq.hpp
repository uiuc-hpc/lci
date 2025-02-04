#ifndef LCI_DATA_STRUCTURE_CQ_CQ_HPP
#define LCI_DATA_STRUCTURE_CQ_CQ_HPP

namespace lci
{
class cq_impl_t : public comp_impl_t
{
 public:
  cq_impl_t(comp_attr_t attr, size_t default_length_ = 8192)
      : comp_impl_t(attr), default_length(default_length_)
  {
    LCT_queue_type_t cq_type = LCT_QUEUE_LCRQ;
    queue = LCT_queue_alloc(cq_type, default_length);
  }
  ~cq_impl_t() { LCT_queue_free(&queue); }
  void signal(status_t status) override
  {
    LCI_Assert(status.error.is_ok(), "status.error is not ok!\n");
    LCI_PCOUNTER_ADD(comp_produce, 1);
    status_t* p = new status_t(status);
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
      status = *p;
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
}  // namespace lci

#endif  // LCI_DATA_STRUCTURE_CQ_CQ_HPP