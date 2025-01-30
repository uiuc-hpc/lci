#ifndef LCIXX_DATA_STRUCTURE_CQ_CQ_HPP
#define LCIXX_DATA_STRUCTURE_CQ_CQ_HPP

namespace lcixx
{
class cq_impl_t : public comp_impl_t
{
 public:
  cq_impl_t(size_t default_length_ = 8192) : default_length(default_length_)
  {
    LCT_queue_type_t cq_type = LCT_QUEUE_LCRQ;
    queue = LCT_queue_alloc(cq_type, default_length);
  }
  ~cq_impl_t() { LCT_queue_free(&queue); }
  void signal(status_t status) override
  {
    LCIXX_PCOUNTER_ADD(comp_produce, 1);
    status_t* p = new status_t(status);
    LCT_queue_push(queue, p);
  }
  bool pop(status_t* status)
  {
    // TODO: let cq directly store status_t instead of a pointer
    status_t* p = static_cast<status_t*>(LCT_queue_pop(queue));
    if (p == nullptr) {
      return false;
    } else {
      *status = *p;
      delete p;
      LCIXX_PCOUNTER_ADD(comp_consume, 1);
      return true;
    }
  }

 private:
  size_t default_length;
  LCT_queue_t queue;
};
}  // namespace lcixx

#endif  // LCIXX_DATA_STRUCTURE_CQ_CQ_HPP