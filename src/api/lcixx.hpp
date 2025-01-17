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

template <typename T>
struct result_t {
  T value;
  bool success;
  T unwrap()
  {
    if (!success) {
      // TODO: replace with LCIXX_Assert
      throw std::runtime_error("Unwrap failed\n");
    }
    return value;
  }
};

template <typename T>
struct result3_t {
  T value;
  enum class status_t {
    success,
    retry,
    fatal,
  } status;
  T unwrap()
  {
    if (status != status_t::success) {
      // TODO: replace with LCIXX_Assert
      throw std::runtime_error("Unwrap failed\n");
    }
    return value;
  }
  bool is_retry() { return status == status_t::retry; }
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
using rkey_t = std::vector<char>;
}  // namespace lcixx

#include "lcixx_binding.hpp"

#endif  // LCIXX_API_LCIXX_HPP