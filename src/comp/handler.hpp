// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_COMP_HANDLER_HPP
#define LCI_COMP_HANDLER_HPP

namespace lci
{
class handler_t : public comp_impl_t
{
 public:
  handler_t(comp_attr_t attr_, comp_handler_t handler)
      : comp_impl_t(attr_), m_handler(handler)
  {
    attr.comp_type = attr_comp_type_t::handler;
  }

  ~handler_t() = default;

  void signal(status_t status) override
  {
    LCI_PCOUNTER_ADD(comp_produce, 1);
    LCI_PCOUNTER_ADD(comp_consume, 1);
    (*m_handler)(std::move(status));
  }

 private:
  comp_handler_t m_handler;
};

}  // namespace lci

#endif  // LCI_COMP_HANDLER_HPP