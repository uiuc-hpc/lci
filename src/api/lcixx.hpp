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
  T value;
  bool is_set;
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

struct default_config_t {
  option_backend_t backend;
  uint64_t packet_size;
  uint64_t packet_num;
  uint64_t backend_max_sends;
  uint64_t backend_max_recvs;
  uint64_t backend_max_cqes;
};
extern default_config_t g_default_config;

// Forward declaration
class runtime_t;
class net_context_t;
class net_device_t;
class net_endpoint_t;

class runtime_impl_t;
class runtime_t
{
 public:
  struct config_t {
    bool use_reg_cache;
    bool use_control_channel;
    option_rdv_protocol_t rdv_protocol;
  };

  int get_rank() const;
  int get_nranks() const;
  config_t get_config() const;
  net_context_t get_default_net_context() const;
  net_device_t get_default_net_device() const;

  runtime_impl_t* p_impl;
};

/**
 * @ingroup LCIXX_RESOURCE
 * @brief Allocate a runtime. Not thread-safe.
 */
class alloc_runtime_x
{
 public:
  runtime_t call();
};

/**
 * @ingroup LCIXX_RESOURCE
 * @brief Free a runtime. Not thread-safe.
 */
class free_runtime_x
{
 public:
  // mandatory
  runtime_t runtime;
  free_runtime_x(runtime_t runtime_) : runtime(runtime_) {}
  void call();
};

/**
 * @defgroup LCIXX_BACKEND_DOMAIN LCIXX Backend Domain
 * @ingroup LCIXX_BACKEND
 * @brief LCIXX backend domain related API.
 */
/**
 * @ingroup LCIXX_BACKEND_DOMAIN
 * @brief The domain type.
 */
class net_context_impl_t;
class net_context_t
{
 public:
  struct config_t {
    option_backend_t backend;
    std::string provider_name;
    int64_t max_msg_size;
  } config;

  config_t get_config() const;

  net_context_impl_t* p_impl;
};

class alloc_net_context_x
{
 public:
  // mandatory
  runtime_t runtime;
  // optional
  option_t<option_backend_t> backend;

  alloc_net_context_x(runtime_t runtime_) : runtime(runtime_) {}
  alloc_net_context_x&& set_backend(option_backend_t backend_)
  {
    backend = backend_;
    return std::move(*this);
  };
  net_context_t call();
};

class free_net_context_x
{
 public:
  net_context_t net_context;
  free_net_context_x&& set_context(net_context_t);
  void call();
};

class net_device_impl_t;
class net_device_t
{
 public:
  struct config_t {
    int64_t max_sends;
    int64_t max_recvs;
    int64_t max_cqes;
  } config;
  net_device_impl_t* p_impl;
};

class alloc_net_device_x
{
 public:
  // mandatory
  runtime_t runtime;
  // optional
  option_t<net_context_t> context;
  option_t<int64_t> max_sends;
  option_t<int64_t> max_recvs;
  option_t<int64_t> max_cqes;

  alloc_net_device_x(runtime_t runtime_) : runtime(runtime_) {}
  alloc_net_device_x&& set_context(net_context_t context_)
  {
    context = context_;
    return std::move(*this);
  };
  alloc_net_device_x&& set_max_sends(int64_t max_sends_)
  {
    max_sends = max_sends_;
    return std::move(*this);
  };
  alloc_net_device_x&& set_max_recvs(int64_t max_recvs_)
  {
    max_recvs = max_recvs_;
    return std::move(*this);
  };
  alloc_net_device_x&& set_max_cqes(int64_t max_cqes_)
  {
    max_cqes = max_cqes_;
    return std::move(*this);
  };
  net_device_t call();
};

class free_net_device_x
{
 public:
  net_device_t device;
  free_net_device_x(net_device_t device_) : device(device_) {}
  void call();
};

// memory region
class mr_impl_t;
class mr_t
{
 public:
  mr_impl_t* p_impl;
};

class register_memory_x
{
 public:
  net_device_t device;
  void* address;
  size_t size;
  register_memory_x(net_device_t device_, void* address_, size_t size_)
      : device(device_), address(address_), size(size_)
  {
  }
  mr_t call();
};

class deregister_memory_x
{
 public:
  mr_t mr;
  deregister_memory_x(mr_t mr_) : mr(mr_) {}
  void call();
};

class net_endpoint_impl_t;
class net_endpoint_t
{
  net_endpoint_impl_t* p_impl;
};

class alloc_net_endpoint_x
{
 public:
  net_endpoint_t call();
};

class free_net_endpoint_x
{
 public:
  net_endpoint_t endpoint;
  void call();
};
}  // namespace lcixx

#endif  // LCIXX_API_LCIXX_HPP