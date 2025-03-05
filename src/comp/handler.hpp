#ifndef LCI_COMP_HANDLER_HPP
#define LCI_COMP_HANDLER_HPP

namespace lci
{
class handler_t : public comp_impl_t
{
 public:
  handler_t(comp_attr_t attr, comp_handler_t handler)
      : comp_impl_t(attr), m_handler(handler)
  {
    attr.comp_type = attr_comp_type_t::handler;
  }

  ~handler_t() = default;

  void signal(status_t status) override { (*m_handler)(std::move(status)); }

 private:
  comp_handler_t m_handler;
};

}  // namespace lci

#endif  // LCI_COMP_HANDLER_HPP