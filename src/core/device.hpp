#ifndef LCIXX_CORE_DEVICE_HPP
#define LCIXX_CORE_DEVICE_HPP

namespace lcixx
{
class device_impl_t
{
 public:
  device_attr_t attr;
  device_impl_t() : attr() {}
  ~device_impl_t() {}
};
}  // namespace lcixx

#endif  // LCIXX_CORE_DEVICE_HPP