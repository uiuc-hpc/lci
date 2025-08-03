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
 * @defgroup LCI_COLL Collective Communication
 * @brief This section describes the LCI API for collective communication.
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
 * operations. It has three categories: done, posted, and retry. The done
 * category indicates that the operation has been completed. The posted category
 * indicates that the operation is posted and the completion will be reported
 * later. The retry category indicates that the operation temporarily failed and
 * the user should retry the operation. Within each category, there are multiple
 * sub error codes offering additional information.
 */
enum class errorcode_t {
  done_min,     /**< boundary marker */
  done,         /**< the operation has been completed */
  done_backlog, /**< the operation has been pushed into a backlog queue and can
                 be considered as completed by users */
  done_max,     /**< boundary marker */
  posted_min,   /**< boundary marker */
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
   * @brief Check if the error code is in the done category.
   * @return true if the error code is in the done category.
   */
  bool is_done() const
  {
    return errorcode > errorcode_t::done_min &&
           errorcode < errorcode_t::done_max;
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
 * @brief The type of broadcast algorithm.
 */
enum class broadcast_algorithm_t {
  none,   /**< automatically select the best algorithm */
  direct, /**< direct algorithm */
  tree,   /**< binomial tree algorithm */
  ring,   /**< ring algorithm */
};

/**
 * @brief Get the string representation of a collective algorithm.
 * @param opcode The collective algorithm.
 * @return The string representation of the collective algorithm.
 */
const char* get_broadcast_algorithm_str(broadcast_algorithm_t algorithm);

/**
 * @ingroup LCI_BASIC
 * @brief The type of reduce scatter algorithm.
 */
enum class reduce_scatter_algorithm_t {
  none,   /**< automatically select the best algorithm */
  direct, /**< direct algorithm */
  tree,   /**< reduce followed by broadcast */
  ring,   /**< ring algorithm */
};

/**
 * @brief Get the string representation of a collective algorithm.
 * @param opcode The collective algorithm.
 * @return The string representation of the collective algorithm.
 */
const char* get_reduce_scatter_algorithm_str(broadcast_algorithm_t algorithm);

/**
 * @ingroup LCI_BASIC
 * @brief The type of allreduce algorithm.
 */
enum class allreduce_algorithm_t {
  none,   /**< automatically select the best algorithm */
  direct, /**< direct algorithm */
  tree,   /**< reduce followed by broadcast */
  ring,   /**< ring algorithm */
};

/**
 * @brief Get the string representation of a collective algorithm.
 * @param opcode The collective algorithm.
 * @return The string representation of the collective algorithm.
 */
const char* get_allreduce_algorithm_str(broadcast_algorithm_t algorithm);

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
 * @brief A special mr_t value for host memory.
 */
const mr_t MR_HOST = mr_t();

/**
 * @ingroup LCI_BASIC
 * @brief A special mr_t value for device memory.
 */
const mr_t MR_DEVICE = mr_t(reinterpret_cast<void*>(0x1));

/**
 * @ingroup LCI_BASIC
 * @brief A special mr_t value for unknown memory. LCI will detect the memory
 * type automatically.
 */
const mr_t MR_UNKNOWN = mr_t(reinterpret_cast<void*>(0x2));

inline bool mr_t::is_empty() const
{
  return reinterpret_cast<uintptr_t>(p_impl) < 3;
}

/**
 * @ingroup LCI_BASIC
 * @brief The type of remote memory region.
 * @details The internal structure of the remote memory region should be
 * considered opaque to users.
 */
struct rmr_t {
  uintptr_t base;
  uint64_t opaque_rkey;
  rmr_t() : base(0), opaque_rkey(0) {}
  bool is_empty() const { return base == 0 && opaque_rkey == 0; }
};

/**
 * @ingroup LCI_BASIC
 * @brief The NULL value of rkey_t.
 */
const rmr_t RMR_NULL = rmr_t();

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
const tag_t ANY_TAG = static_cast<tag_t>(-1);

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
/**
 * @ingroup LCI_BASIC
 * @brief The type of matching engine entry key.
 */
using matching_entry_key_t = uint64_t;
/**
 * @ingroup LCI_BASIC
 * @brief The type of matching engine entry value.
 */
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
  // TODO: rename buffer -> memory
  buffer,  /**< When the local buffers can be written or freed */
  network, /**< When the associated network-layer operation is completed.  */
};

/**
 * @ingroup LCI_BASIC
 * @brief The user-defined allocator type.
 */
struct allocator_base_t {
  virtual void* allocate(size_t size) = 0;
  virtual void deallocate(void* ptr) = 0;
  virtual ~allocator_base_t() = default;
};

struct allocator_default_t : public allocator_base_t {
  void* allocate(size_t size) { return malloc(size); }
  void deallocate(void* ptr) { free(ptr); }
};
extern allocator_default_t g_allocator_default;

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
  buffer_t() : base(nullptr), size(0) {}
  buffer_t(void* base_, size_t size_) : base(base_), size(size_) {}
};

/**
 * @ingroup LCI_BASIC
 * @brief The type of the completion desciptor for a posted communication.
 */
struct status_t {
  error_t error = errorcode_t::retry_init;
  int rank = -1;
  void* buffer = nullptr;
  size_t size = 0;
  tag_t tag = 0;
  void* user_context = nullptr;
  status_t() = default;
  status_t(errorcode_t error_) : error(error_) {}
  explicit status_t(void* user_context_)
      : error(errorcode_t::done), user_context(user_context_)
  {
  }
  void set_done() { error = errorcode_t::done; }
  void set_posted() { error = errorcode_t::posted; }
  void set_retry() { error = errorcode_t::retry; }
  bool is_done() const { return error.is_done(); }
  bool is_posted() const { return error.is_posted(); }
  bool is_retry() const { return error.is_retry(); }
  error_t get_error() const { return error; }
  int get_rank() const { return rank; }
  void* get_buffer() const { return buffer; }
  size_t get_size() const { return size; }
  tag_t get_tag() const { return tag; }
  void* get_user_context() const { return user_context; }
};

/**
 * @ingroup LCI_BASIC
 * @brief Special completion object setting `allow_posted` to false.
 */
const comp_t COMP_NULL = comp_t(reinterpret_cast<comp_impl_t*>(0x0));

/**
 * @ingroup LCI_BASIC
 * @brief Deprecated. Same as COMP_NULL.
 */
const comp_t COMP_NULL_EXPECT_DONE =
    comp_t(reinterpret_cast<comp_impl_t*>(0x0));

/**
 * @ingroup LCI_BASIC
 * @brief Special completion object setting `allow_posted` and `allow_retry` to
 * false.
 */
const comp_t COMP_NULL_RETRY = comp_t(reinterpret_cast<comp_impl_t*>(0x1));

/**
 * @ingroup LCI_BASIC
 * @brief Deprecated. Same as COMP_NULL_RETRY.
 */
const comp_t COMP_NULL_EXPECT_DONE_OR_RETRY =
    comp_t(reinterpret_cast<comp_impl_t*>(0x1));

inline bool comp_t::is_empty() const
{
  return reinterpret_cast<uintptr_t>(p_impl) <= 1;
}

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
  comp_impl_t() = default;
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
using graph_node_run_cb_t = status_t (*)(void* value);

/**
 * @ingroup LCI_BASIC
 * @brief A dummy callback function for a graph node.
 * @details This function can be used as a placeholder for a graph node that
 * does not perform any operation.
 */
const graph_node_run_cb_t GRAPH_NODE_DUMMY_CB = nullptr;

/**
 * @ingroup LCI_BASIC
 * @brief The function signature for a callback that will be triggered when the
 * node was freed.
 */
using graph_node_free_cb_t = void (*)(void* value);

/**
 * @ingroup LCI_BASIC
 * @brief The function signature for a edge funciton in the completion graph.
 */
using graph_edge_run_cb_t = void (*)(status_t status, void* src_value,
                                     void* dst_value);

}  // namespace lci

#include "lci_binding_post.hpp"

namespace lci
{
/***********************************************************************
 * Overloading graph_add_node for functor
 **********************************************************************/
#if __cplusplus >= 201703L
template <typename T>
status_t graph_execute_op_fn(void* value)
{
  auto op = static_cast<T*>(value);
  using result_t = std::invoke_result_t<T>;

  if constexpr (std::is_same_v<result_t, status_t>) {
    status_t result = (*op)();
    return result;
  } else if constexpr (std::is_same_v<result_t, errorcode_t>) {
    errorcode_t result = (*op)();
    return result;
  } else {
    (*op)();
    return errorcode_t::done;
  }
}
#else
// Specialization for status_t return type
template <typename T>
typename std::enable_if<
    std::is_same<typename std::result_of<T()>::type, status_t>::value,
    status_t>::type
graph_execute_op_fn(void* value)
{
  auto op = static_cast<T*>(value);
  status_t result = (*op)();
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
  errorcode_t result = (*op)();
  return result;
}

// Specialization for all other return types
template <typename T>
typename std::enable_if<
    !std::is_same<typename std::result_of<T()>::type, status_t>::value &&
        !std::is_same<typename std::result_of<T()>::type, errorcode_t>::value,
    status_t>::type
graph_execute_op_fn(void* value)
{
  auto op = static_cast<T*>(value);
  (*op)();
  return errorcode_t::done;
}
#endif

template <typename T>
void graph_free_op_fn(void* value)
{
  auto op = static_cast<T*>(value);
  delete op;
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
  graph_node_run_cb_t wrapper = graph_execute_op_fn<T>;
  T* fn = new T(op);
  graph_node_free_cb_t free_cb = graph_free_op_fn<T>;
  auto ret = graph_add_node_x(graph, wrapper)
                 .value(reinterpret_cast<void*>(fn))
                 .free_cb(free_cb)();
  fn->user_context(ret);
  return ret;
}

}  // namespace lci

#endif  // LCI_API_LCI_HPP