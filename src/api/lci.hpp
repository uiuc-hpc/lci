// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_API_LCI_HPP
#define LCI_API_LCI_HPP

#include <memory>
#include <stdexcept>
#include <vector>
#include <string>
#include <cstring>
#include <functional>

#include "lci_config.hpp"

/**
 * @defgroup LCI_BASIC Basic Concepts
 * @brief This section describes basic concepts in LCI.
 */

/**
 * @defgroup LCI_SETUP Setup and Teardown
 * @brief This section describes setup and teardown API.
 */

/**
 * @defgroup LCI_RESOURCE Resource Management
 * @brief This section describes the LCI API for resource management.
 */

/**
 * @defgroup LCI_MEMORY Memory Registration
 * @brief This section describes the LCI API for memory registration.
 */

/**
 * @defgroup LCI_NET Network-Layer Communication
 * @brief This section describes the LCI API for network-layer communication.
 */

/**
 * @defgroup LCI_COMM Communication Posting
 * @brief This section describes the LCI API for posting communication.
 */

/**
 * @defgroup LCI_COMPLETION Completion Checking
 * @brief This section describes the LCI API for checking the completion status
 * of posted communications.
 */

/**
 * @namespace lci
 * @brief All LCI API functions and classes are defined in this namespace.
 */
namespace lci
{
enum class attr_backend_t {
  none,
  ibv,
  ofi,
  ucx,
};

enum attr_net_lock_mode_t {
  LCI_NET_TRYLOCK_SEND = 1,
  LCI_NET_TRYLOCK_RECV = 1 << 1,
  LCI_NET_TRYLOCK_POLL = 1 << 2,
  LCI_NET_TRYLOCK_MAX = 1 << 3,
};

// mimic std::optional as we don't want to force c++17 for now
template <typename T>
struct option_t {
  option_t() : m_value(), m_is_set(false) {}
  option_t(T value_) : m_value(value_), m_is_set(true) {}
  option_t(T value_, bool is_set_)
      : m_value(value_), m_is_set(is_set_) {}  // set default value
  T get_value_or(T default_value) const
  {
    return m_is_set ? m_value : default_value;
  }
  bool get_set_value(T* value) const
  {
    if (m_is_set) {
      *value = this->m_value;
      return true;
    }
    return false;
  }
  T get_value() const { return m_value; }
  bool is_set() const { return m_is_set; }
  operator T() const { return m_value; }
  T m_value;
  bool m_is_set;
};
}  // namespace lci

#include "lci_binding_pre.hpp"

namespace lci
{
/**
 * @brief The actual error code for LCI API functions.
 * @ingroup LCI_BASIC
 * @details The error code is used to indicate the status of certain LCI
 * operations. It has three categories: ok, posted, and retry. The ok category
 * indicates that the operation has been completed. The posted category
 * indicates that the operation is posted and the completion will be reported
 * later. The retry category indicates that the operation temporarily failed and
 * the user should retry the operation. Within each category, there are multiple
 * sub error codes offering additional information.
 */
enum class errorcode_t {
  ok_min,     /**< boundary marker */
  ok,         /**< the operation has been completed */
  ok_backlog, /**< the operation has been pushed into a backlog queue and can be
                 considered as completed by users */
  ok_max,     /**< boundary marker */
  posted_min, /**< boundary marker */
  posted, /**< the operation is posted and the completion will be reported later
           */
  posted_backlog, /**< the operation has been pushed into a backlog queue and
                     can be considered as posted by users  */
  posted_max,     /**< boundary marker */
  retry_min,      /**< boundary marker */
  retry, /**< the operation temporarily failed and the user should retry the
            operation */
  retry_init, /**< the default value for the error code */
  retry_lock, /**< the operation temporarily failed due to lock contention */
  retry_nopacket, /**< the operation temporarily failed due to the lack of
                     packets */
  retry_nomem,   /**< the operation temporarily failed because the network queue
                    is full */
  retry_backlog, /**< the operation temporarily failed because the backlog queue
                    is not empty */
  retry_max,     /**< boundary marker */
  fatal, /**< placeholder. Not used for now. All fatal error are reported
            through C++ std::runtime_error. */
};

/**
 * @brief Get the string representation of an error code.
 * @param errorcode The error code to be converted to string.
 * @return The string representation of the error code.
 */
const char* get_errorcode_str(errorcode_t errorcode);

/**
 * @brief Wrapper class for error code.
 * @ingroup LCI_BASIC
 * @details This class wraps the error code and provides utility functions to
 * check the error code.
 */
struct error_t {
  errorcode_t errorcode;
  error_t() : errorcode(errorcode_t::retry_init) {}
  /**
   * @brief Construct an error_t object with a specific error code.
   * @param errorcode_ The error code to be wrapped.
   */
  error_t(errorcode_t errorcode_) : errorcode(errorcode_) {}
  /**
   * @brief Reset the error code to retry.
   */
  void reset_retry() { errorcode = errorcode_t::retry; }
  /**
   * @brief Check if the error code is in the ok category.
   * @return true if the error code is in the ok category.
   */
  bool is_ok() const
  {
    return errorcode > errorcode_t::ok_min && errorcode < errorcode_t::ok_max;
  }
  /**
   * @brief Check if the error code is in the posted category.
   * @return true if the error code is in the posted category.
   */
  bool is_posted() const
  {
    return errorcode > errorcode_t::posted_min &&
           errorcode < errorcode_t::posted_max;
  }
  /**
   * @brief Check if the error code is in the retry category.
   * @return true if the error code is in the retry category.
   */
  bool is_retry() const
  {
    return errorcode > errorcode_t::retry_min &&
           errorcode < errorcode_t::retry_max;
  }
  /**
   * @brief Get the string representation of the error code.
   * @return The string representation of the error code.
   */
  const char* get_str() const { return lci::get_errorcode_str(errorcode); }
};

/**
 * @brief The Type of network communication operation codes.
 * @ingroup LCI_BASIC
 */
enum class net_opcode_t {
  SEND,         /**< send */
  RECV,         /**< receive */
  WRITE,        /**< write */
  REMOTE_WRITE, /**< remote write */
  READ,         /**< read */
};

/**
 * @brief Get the string representation of a network operation code.
 * @param opcode The network operation code.
 * @return The string representation of the network operation code.
 */
const char* get_net_opcode_str(net_opcode_t opcode);

/**
 * @ingroup LCI_BASIC
 * @brief The type of network-layer immediate data field.
 * @details The immediate data field is used to carry small data in the network
 */
using net_imm_data_t = uint32_t;

/**
 * @ingroup LCI_BASIC
 * @brief The struct for network status.
 * @details A network status is used to describe a completed network
 * communication operation.
 */
struct net_status_t {
  net_opcode_t opcode;
  int rank;
  void* user_context;
  size_t length;
  net_imm_data_t imm_data;
};

/**
 * @ingroup LCI_BASIC
 * @brief The type of remote memory region key.
 */
using rkey_t = uint64_t;

/**
 * @ingroup LCI_BASIC
 * @brief The type of tag.
 */
using tag_t = uint64_t;

/**
 * @ingroup LCI_BASIC
 * @brief The enum class of comunication direction.
 */
enum class direction_t {
  OUT, /**< push data out, such as send/active message/put */
  IN,  /**< pull data in, such as receive/get */
};

/**
 * @ingroup LCI_BASIC
 * @brief The type of remote completion handler.
 * @details A remote completion handler is used to address a completion object
 * on a remote process.
 */
using rcomp_t = uint32_t;

/**
 * @ingroup LCI_BASIC
 * @brief Special rank value for any-source receive.
 */
const int ANY_SOURCE = -1;

/**
 * @ingroup LCI_BASIC
 * @brief Special tag value for any-tag receive.
 */
const tag_t ANY_TAG = -1;

/**
 * @ingroup LCI_BASIC
 * @brief The type of matching entry.
 * @details A matching entry can be an incoming send or a posted receive.
 */
enum class matching_entry_type_t : unsigned {
  send = 0, /**< incoming send */
  recv = 1, /**< posted receive */
};

/**
 * @ingroup LCI_BASIC
 * @brief Enum class for matching policy.
 */
enum class matching_policy_t : unsigned {
  none = 0,      /**< match any send with any receive */
  rank_only = 1, /**< match by rank */
  tag_only = 2,  /**< match by tag */
  rank_tag = 3,  /**< match by rank and tag */
  max = 4,       /**< boundary marker */
};

using matching_entry_key_t = uint64_t;
using matching_entry_val_t = void*;

/**
 * @ingroup LCI_BASIC
 * @brief The enum class of completion semantic.
 * @details The completion semantic is used to define when a posted
 * communication with *OUT* direction is considered as completed. For *IN*
 * direction, a communication is considered as completed when the data has been
 * written into the local buffer.
 */
enum class comp_semantic_t {
  buffer,  /**< When the local buffers can be written or freed */
  network, /**< When the associated network-layer operation is completed.  */
};

/**
 * @ingroup LCI_BASIC
 * @brief The type of a local buffer descriptor.
 * @details A buffer descriptor does not *own* the buffer, i.e. it will never
 * copy or free the buffer. Users are responsible for managing the lifecycle of
 * the memory buffer being described.
 */
struct buffer_t {
  void* base;  /**< The base address of the buffer */
  size_t size; /**< The size of the buffer */
  mr_t mr;     /**< The registered memory region of the buffer */
  buffer_t() : base(nullptr), size(0), mr() {}
  buffer_t(void* base_, size_t size_) : base(base_), size(size_) {}
  buffer_t(void* base_, size_t size_, mr_t mr_)
      : base(base_), size(size_), mr(mr_)
  {
  }
};

/**
 * @ingroup LCI_BASIC
 * @brief The type of a remote buffer descriptor.
 */
struct rbuffer_t {
  uintptr_t base; /**< The base address of the remote buffer */
  rkey_t rkey;    /**< The remote memory region key */
  rbuffer_t() : base(0), rkey(0) {}
  rbuffer_t(uintptr_t base_) : base(base_) {}
  rbuffer_t(uintptr_t base_, rkey_t rkey_) : base(base_), rkey(rkey_) {}
};
using buffers_t = std::vector<buffer_t>;
using rbuffers_t = std::vector<rbuffer_t>;

/**
 * @ingroup LCI_BASIC
 * @brief A generic type for describing or holding data.
 * @details This type is used to pass data from LCI to users through the
 * completion checking APIs. It is included in the @ref status_t struct. A
 * *data* object either describes or owns the underlying memory buffer(s). It
 * will own the memory buffer(s) if the data is allocated by the LCI runtime
 * (a.k.a the receive buffer(s) of active messages).
 *
 * If the data object owns the memory buffer(s), users can use the
 * `data_t::get_scalar/buffer/buffers` to get the data. `get_buffer/buffers`
 * provides three semantics to optimize the memory copy behavior, defined by
 * @ref data_t::get_semantic_t
 * - *copy*: the data object will allocate new buffer(s) and copy the data from
 * the original buffer(s) to the new buffer(s). The returned buffers needed to
 * be freed by users through `free()`. Users can call
 * `data_t::get_scalar/buffer/buffers()` again.
 * - *move*: the data object will directly return the buffer(s) and users will
 * own them afterwards. The returned buffers needed to be freed by users through
 * `free()`. Users cannot call `data_t::get_scalar/buffer/buffers()` anymore.
 * - *view*: the data object will directly return the buffer(s) but they are not
 * owned by users. Users cannot access them once the `data` object goes out of
 * scope. Users cannot free the buffers. Users can call
 * `data_t::get_scalar/buffer/buffers()` again.
 *
 * `get_scalar` always uses the *copy* semantic.
 */
struct data_t {
  static const int MAX_SCALAR_SIZE = 23;
  enum class type_t : unsigned int {
    none,
    scalar,
    buffer,
    buffers,
  };
  // FIXME: It is a undefined behavior to access multiple union members at the
  // same time
  union {
    struct {
      // common fields
      type_t type : 2;
      bool own_data : 1;
    } common;
    struct {
      type_t type : 2;
      bool own_data : 1;
      uint8_t size : 5;
      char data[MAX_SCALAR_SIZE];
    } scalar;
    struct {
      type_t type : 2;
      bool own_data : 1;
      size_t size : 61;
      void* base;
      mr_t mr;
    } buffer;
    struct {
      type_t type : 2;
      bool own_data : 1;
      size_t count;
      buffer_t* buffers;
    } buffers;
  };
  data_t();
  data_t(buffer_t buffer_, bool own_data_ = false);
  data_t(buffers_t buffers_, bool own_data_ = false);
  data_t(size_t size);
  data_t(size_t sizes[], int count);
  data_t(const data_t& other);
  data_t(data_t&& other);
  data_t& operator=(data_t other);
  friend void swap(data_t& first, data_t& second);

  type_t get_type() const { return common.type; }
  void set_type(type_t type_) { common.type = type_; }
  bool get_own_data() const { return common.own_data; }
  void set_own_data(bool own_data_) { common.own_data = own_data_; }

  bool is_scalar() const { return get_type() == type_t::scalar; }
  bool is_buffer() const { return get_type() == type_t::buffer; }
  bool is_buffers() const { return get_type() == type_t::buffers; }

  void copy_from(const void* data_, size_t size);
  void copy_from(const buffers_t& buffers_);

  /**
   * @brief Enum class of get semantic.
   * @details The get semantic is used to optimize the memory copy behavior when
   * users get the data from the `data` object.
   */
  enum class get_semantic_t {
    move, /**< need to free the returned buffer; can only call get once. */
    copy, /**< need to free the returned buffer; can call get multiple times. */
    view, /**< no need to free the returned buffer; can call get multiple times.
           */
  };

  /**
   * @brief Get the scalar data from the `data` object.
   * @tparam T The type of the scalar data.
   * @return The scalar data.
   * @details The function always uses the copy semantic.
   */
  template <typename T>
  T get_scalar(/* always copy semantic */) const;
  /**
   * @brief Get the buffer from the `data` object.
   * @param semantic The get semantic to use.
   * @return The buffer.
   */
  buffer_t get_buffer(get_semantic_t semantic = get_semantic_t::move);
  /**
   * @brief Get the number of buffers from the `data` object.
   * @return The number of buffers.
   */
  size_t get_buffers_count() const;
  /**
   * @brief Get the buffers from the `data` object.
   * @param semantic The get semantic to use.
   * @return The buffers.
   */
  buffers_t get_buffers(get_semantic_t semantic = get_semantic_t::move);
};

/**
 * @ingroup LCI_BASIC
 * @brief The type of the completion desciptor for a posted communication.
 */
struct status_t {
  error_t error = errorcode_t::retry_init;
  int rank = -1;
  data_t data;
  tag_t tag = 0;
  void* user_context = nullptr;
  status_t() = default;
  status_t(errorcode_t error_) : error(error_) {}
  explicit status_t(void* user_context_)
      : error(errorcode_t::ok), user_context(user_context_)
  {
  }
};

/**
 * @ingroup LCI_BASIC
 * @brief Special completion object setting `allow_posted` to false.
 */
const comp_t COMP_NULL_EXPECT_OK = comp_t(reinterpret_cast<comp_impl_t*>(0x1));

/**
 * @ingroup LCI_BASIC
 * @brief Special completion object setting `allow_posted` and `allow_retry` to
 * false.
 */
const comp_t COMP_NULL_EXPECT_OK_OR_RETRY =
    comp_t(reinterpret_cast<comp_impl_t*>(0x2));

/**
 * @ingroup LCI_BASIC
 * @brief Completion object implementation base type.
 * @details Users can overload the `signal` function to implement their own
 * completion object.
 */
class comp_impl_t
{
 public:
  using attr_t = comp_attr_t;
  comp_impl_t(const attr_t& attr_) : attr(attr_) {}
  virtual ~comp_impl_t() = default;
  virtual void signal(status_t) = 0;
  comp_attr_t attr;
};

/**
 * @ingroup LCI_BASIC
 * @brief Function Signature for completion handler.
 */
using comp_handler_t = void (*)(status_t status);

/**
 * @ingroup LCI_BASIC
 * @brief The user-defined reduction operation.
 * @details `right` and `dst` can be the same pointer.
 */
using reduce_op_t = void (*)(const void* left, const void* right, void* dst,
                             size_t n);

/**
 * @ingroup LCI_BASIC
 * @brief The node type for the completion graph.
 */
using graph_node_t = void*;

/**
 * @ingroup LCI_BASIC
 * @brief The start node of the completion graph.
 */
const graph_node_t GRAPH_START = reinterpret_cast<graph_node_t>(0x1);
const graph_node_t GRAPH_END = reinterpret_cast<graph_node_t>(0x2);

/**
 * @ingroup LCI_BASIC
 * @brief The function signature for a node function in the completion graph.
 * @details The function should return true if the node is considered completed.
 */
using graph_node_fn_t = status_t (*)(void* value);

/**
 * @ingroup LCI_BASIC
 * @brief The function signature for a edge funciton in the completion graph.
 */
using graph_edge_fn_t = void (*)(status_t status, void* src_value,
                                 void* dst_value);

}  // namespace lci

#include "lci_binding_post.hpp"

namespace lci
{
/***********************************************************************
 * Overloading graph_add_node for functor
 **********************************************************************/

// template <typename T>
// status_t graph_execute_op_fn(void* value)
// {
//   auto op = static_cast<T*>(value);
//   auto ret = op->operator()();  // call the stored functor
//   delete op;
//   return ret;
// }

// Specialization status_t return type
template <typename T>
typename std::enable_if<
    std::is_same<typename std::result_of<T()>::type, status_t>::value,
    status_t>::type
graph_execute_op_fn(void* value)
{
  auto op = static_cast<T*>(value);
  status_t result = op->operator()();
  delete op;
  return result;
}

// Specialization for errorcode_t return type
template <typename T>
typename std::enable_if<
    std::is_same<typename std::result_of<T()>::type, errorcode_t>::value,
    status_t>::type
graph_execute_op_fn(void* value)
{
  auto op = static_cast<T*>(value);
  errorcode_t result = op->operator()();
  delete op;
  return result;
}

// Specialization for other return types
template <typename T>
typename std::enable_if<
    !std::is_same<typename std::result_of<T()>::type, status_t>::value,
    status_t>::type
graph_execute_op_fn(void* value)
{
  auto op = static_cast<T*>(value);
  op->operator()();
  delete op;
  return errorcode_t::ok;
}

/**
 * @ingroup LCI_BASIC
 * @brief Add a functor as a node to the completion graph.
 * @tparam T The type of the functor.
 * @param graph The completion graph.
 * @param op The functor to be added.
 * @return The node added to the completion graph.
 */
template <typename T>
graph_node_t graph_add_node_op(comp_t graph, const T& op)
{
  graph_node_fn_t wrapper = graph_execute_op_fn<T>;
  T* fn = new T(op);
  auto ret =
      graph_add_node_x(graph, wrapper).value(reinterpret_cast<void*>(fn))();
  fn->user_context(ret);
  return ret;
}

/***********************************************************************
 * Some inline implementation
 **********************************************************************/
inline data_t::data_t()
{
  set_type(type_t::none);
  set_own_data(false);
  static_assert(sizeof(data_t) == 24, "data_t size is not 24 bytes");
}
inline data_t::data_t(buffer_t buffer_, bool own_data_)
{
  set_type(type_t::buffer);
  set_own_data(own_data_);
  buffer.base = buffer_.base;
  buffer.size = buffer_.size;
  buffer.mr = buffer_.mr;
}
inline data_t::data_t(buffers_t buffers_, bool own_data_)
{
  set_type(type_t::buffers);
  set_own_data(own_data_);
  buffers.count = buffers_.size();
  buffers.buffers = new buffer_t[buffers.count];
  for (size_t i = 0; i < buffers.count; i++) {
    buffers.buffers[i] = buffers_[i];
  }
}
inline data_t::data_t(size_t size)
{
  set_type(type_t::buffer);
  set_own_data(true);
  buffer.base = malloc(size);
  buffer.size = size;
  buffer.mr = mr_t();
}
inline data_t::data_t(size_t sizes[], int count)
{
  set_type(type_t::buffers);
  set_own_data(true);
  buffers.count = count;
  buffers.buffers = new buffer_t[buffers.count];
  for (int i = 0; i < count; i++) {
    buffers.buffers[i].base = malloc(sizes[i]);
    buffers.buffers[i].size = sizes[i];
    buffers.buffers[i].mr = mr_t();
  }
}
// copy constructor
inline data_t::data_t(const data_t& other)
{
  set_type(other.get_type());
  bool own_data = other.get_own_data();
  set_own_data(own_data);
  if (own_data)
    fprintf(stderr, "Copying buffer with own_data=true is not recommended\n");
  if (other.is_scalar()) {
    scalar.size = other.scalar.size;
    memcpy(scalar.data, other.scalar.data, scalar.size);
  } else if (other.is_buffer()) {
    buffer = other.buffer;
    if (own_data) {
      buffer.base = malloc(other.buffer.size);
      memcpy(buffer.base, other.buffer.base, other.buffer.size);
    }
  } else if (other.is_buffers()) {
    buffers.count = other.buffers.count;
    buffers.buffers = new buffer_t[buffers.count];
    for (size_t i = 0; i < buffers.count; i++) {
      buffers.buffers[i] = other.buffers.buffers[i];
      if (own_data) {
        buffers.buffers[i].base = malloc(other.buffers.buffers[i].size);
        memcpy(buffers.buffers[i].base, other.buffers.buffers[i].base,
               other.buffers.buffers[i].size);
      }
    }
  }
}
// move constructor
inline data_t::data_t(data_t&& other)
{
  set_type(other.get_type());
  set_own_data(other.get_own_data());
  if (other.is_scalar()) {
    scalar.size = other.scalar.size;
    memcpy(scalar.data, other.scalar.data, scalar.size);
  } else if (other.is_buffer()) {
    buffer = other.buffer;
    other.set_own_data(false);
  } else if (other.is_buffers()) {
    buffers.count = other.buffers.count;
    buffers.buffers = other.buffers.buffers;
    other.set_own_data(false);
    other.buffers.buffers = nullptr;
  }
}
// generic assignment operator
inline data_t& data_t::operator=(data_t other)
{
  swap(*this, other);
  return *this;
}

inline void swap(data_t& first, data_t& second)
{
  char* buf = (char*)malloc(sizeof(data_t));
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
  memcpy(buf, &first, sizeof(data_t));
  memcpy(&first, &second, sizeof(data_t));
  memcpy(&second, buf, sizeof(data_t));
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
  free(buf);
}

inline void data_t::copy_from(const void* data_, size_t size)
{
  if (size <= MAX_SCALAR_SIZE) {
    set_type(type_t::scalar);
    set_own_data(true);
    scalar.size = size;
    memcpy(scalar.data, data_, size);
  } else {
    set_type(type_t::buffer);
    set_own_data(true);
    buffer.base = malloc(size);
    memcpy(buffer.base, data_, size);
    buffer.size = size;
  }
}

inline void data_t::copy_from(const buffers_t& buffers_)
{
  set_type(type_t::buffers);
  set_own_data(true);
  buffers.count = buffers_.size();
  buffers.buffers = new buffer_t[buffers.count];
  for (size_t i = 0; i < buffers.count; i++) {
    buffers.buffers[i].size = buffers_[i].size;
    buffers.buffers[i].base = malloc(buffers_[i].size);
    memcpy(buffers.buffers[i].base, buffers_[i].base, buffers_[i].size);
  }
}

template <typename T>
T data_t::get_scalar(/* always copy semantic */) const
{
  // We still keep the ownership of the data
  if (is_buffer()) {
    if (buffer.size != sizeof(T)) {
      throw std::runtime_error("Buffer size does not match scalar size");
    }
    return *reinterpret_cast<const T*>(buffer.base);
  } else if (is_scalar()) {
    if (sizeof(T) > scalar.size) {
      throw std::runtime_error("No enough data to fit the scalar.");
    }
    return *reinterpret_cast<const T*>(scalar.data);
  } else {
    throw std::runtime_error("Cannot convert to a scalar");
  }
}

inline buffer_t data_t::get_buffer(get_semantic_t semantic)
{
  buffer_t ret;
  if (is_scalar()) {
    if (semantic == get_semantic_t::move || semantic == get_semantic_t::copy) {
      ret.size = scalar.size;
      ret.base = malloc(scalar.size);
      memcpy(ret.base, scalar.data, scalar.size);
    } else {
      ret = buffer_t(scalar.data, scalar.size);
    }
  } else if (is_buffer()) {
    if (semantic == get_semantic_t::copy && get_own_data()) {
      ret.size = buffer.size;
      ret.base = malloc(buffer.size);
      memcpy(ret.base, buffer.base, buffer.size);
    } else {
      ret = buffer_t(buffer.base, buffer.size, buffer.mr);
    }
  } else {
    throw std::runtime_error("Cannot convert to a buffer");
  }
  if (semantic == get_semantic_t::move) {
    set_own_data(false);
    set_type(type_t::none);
  }
  return ret;
}

inline size_t data_t::get_buffers_count() const
{
  if (!is_buffers()) {
    throw std::runtime_error("Not a buffers");
  }
  return buffers.count;
}

inline buffers_t data_t::get_buffers(get_semantic_t semantic)
{
  if (!is_buffers()) {
    throw std::runtime_error("Not buffers");
  }
  if (semantic == get_semantic_t::move) {
    set_own_data(false);
    set_type(type_t::none);
  }
  buffers_t ret;
  for (size_t i = 0; i < buffers.count; i++) {
    if (semantic == get_semantic_t::copy && get_own_data()) {
      buffer_t buffer;
      buffer.size = buffers.buffers[i].size;
      buffer.base = malloc(buffer.size);
      memcpy(buffer.base, buffers.buffers[i].base, buffer.size);
      ret.push_back(buffer);
    } else {
      ret.push_back(buffers.buffers[i]);
    }
  }
  return ret;
}

}  // namespace lci

#endif  // LCI_API_LCI_HPP