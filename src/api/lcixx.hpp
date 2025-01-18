#ifndef LCIXX_API_LCIXX_HPP
#define LCIXX_API_LCIXX_HPP

#include <memory>

#include "lcixx_config.hpp"

/**
 * @defgroup LCIXX_API Lightweight Communication Interface++ (LCI++) API
 * @brief This section describes LCI++ API.
 */

namespace lcixx
{
// mimic std::optional as we don't want to force c++17 for now
template <typename T>
struct option_t {
  option_t() : value(), is_set(false) {}
  option_t(T value_) : value(value_), is_set(true) {}
  T get_value_or(T default_value) const
  {
    return is_set ? value : default_value;
  }
  bool get_value(T* value) const
  {
    if (is_set) {
      *value = this->value;
      return true;
    }
    return false;
  }
  T value;
  bool is_set;
};

enum class errorcode_t {
  ok,
  init,
  retry_min,
  retry,
  retry_lock,
  retry_nomem,
  retry_max,
  fatal_min,
  fatal,
  fatal_max
};

struct error_t {
  errorcode_t errorcode;
  error_t() : errorcode(errorcode_t::init) {}
  error_t(errorcode_t errorcode_) : errorcode(errorcode_) {}
  void reset(errorcode_t errorcode_ = errorcode_t::init)
  {
    errorcode = errorcode_;
  }
  bool is_ok() const { return errorcode == errorcode_t::ok; }
  bool is_retry() const
  {
    return errorcode >= errorcode_t::retry_min &&
           errorcode < errorcode_t::retry_max;
  }
  bool is_fatal() const
  {
    return errorcode >= errorcode_t::fatal_min &&
           errorcode < errorcode_t::fatal_max;
  }
  void assert_no_fatal() const
  {
    if (is_fatal()) throw std::runtime_error("Fatal error encountered");
  }
};

enum class option_backend_t {
  none,
  ibv,
  ofi,
  ucx,
};

enum class option_rdv_protocol_t {
  write,
  writeimm,
};

enum option_net_lock_mode_t {
  LCIXX_NET_TRYLOCK_SEND = 1,
  LCIXX_NET_TRYLOCK_RECV = 1 << 1,
  LCIXX_NET_TRYLOCK_POLL = 1 << 2,
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
  void* ctx;
  size_t length;
  net_imm_data_t imm_data;
};
using rkey_t = uint64_t;
}  // namespace lcixx

#include "lcixx_binding.hpp"

#endif  // LCIXX_API_LCIXX_HPP