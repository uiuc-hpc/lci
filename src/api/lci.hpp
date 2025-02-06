#ifndef LCI_API_LCI_HPP
#define LCI_API_LCI_HPP

#include <memory>
#include <stdexcept>
#include <string>

#include "lci_config.hpp"

/**
 * @defgroup LCI_API Lightweight Communication Interface (LCI) API
 * @brief This section describes LCI API.
 */

namespace lci
{
// mimic std::optional as we don't want to force c++17 for now
template <typename T>
struct option_t {
  option_t() : value(), is_set(false) {}
  option_t(T value_) : value(value_), is_set(true) {}
  option_t(T value_, bool is_set_)
      : value(value_), is_set(is_set_) {}  // set default value
  T get_value_or(T default_value) const
  {
    return is_set ? value : default_value;
  }
  bool get_set_value(T* value) const
  {
    if (is_set) {
      *value = this->value;
      return true;
    }
    return false;
  }
  T get_value() const { return value; }
  operator T() const { return value; }
  T value;
  bool is_set;
};

enum class errorcode_t {
  ok,
  posted,
  retry_min,
  retry,
  retry_init,
  retry_lock,
  retry_nomem,
  retry_max,
  fatal,
};

struct error_t {
  errorcode_t errorcode;
  error_t() : errorcode(errorcode_t::retry_init) {}
  error_t(errorcode_t errorcode_) : errorcode(errorcode_) {}
  void reset(errorcode_t errorcode_ = errorcode_t::retry_init)
  {
    errorcode = errorcode_;
  }
  bool is_ok() const { return errorcode == errorcode_t::ok; }
  bool is_posted() const { return errorcode == errorcode_t::posted; }
  bool is_retry() const
  {
    return errorcode >= errorcode_t::retry_min &&
           errorcode < errorcode_t::retry_max;
  }
};

enum class option_backend_t {
  none,
  ibv,
  ofi,
  ucx,
};

enum option_net_lock_mode_t {
  LCI_NET_TRYLOCK_SEND = 1,
  LCI_NET_TRYLOCK_RECV = 1 << 1,
  LCI_NET_TRYLOCK_POLL = 1 << 2,
  LCI_NET_TRYLOCK_MAX = 1 << 3,
};

// net cq entry
enum class net_opcode_t {
  SEND,
  RECV,
  WRITE,
  REMOTE_WRITE,
  READ,
};
using net_imm_data_t = uint32_t;
struct net_status_t {
  net_opcode_t opcode;
  int rank;
  void* user_context;
  size_t length;
  net_imm_data_t imm_data;
};
using rkey_t = uint64_t;
using tag_t = uint32_t;
enum class direction_t {
  SEND,
  RECV,
};
using rcomp_t = uint32_t;
struct status_t {
  error_t error = errorcode_t::retry_init;
  int rank = -1;
  void* buffer = nullptr;
  size_t size = 0;
  tag_t tag = 0;
  void* user_context = nullptr;
};
}  // namespace lci

#include "lci_binding.hpp"

namespace lci
{
class comp_impl_t
{
 public:
  using attr_t = comp_attr_t;
  comp_impl_t(const attr_t& attr_) : attr(attr_) {}
  virtual ~comp_impl_t() = default;
  virtual void signal(status_t) = 0;
  comp_attr_t attr;
};
}  // namespace lci

#endif  // LCI_API_LCI_HPP