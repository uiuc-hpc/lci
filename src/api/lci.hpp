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
  retry_nopacket,
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
using tag_t = uint64_t;
enum class direction_t {
  OUT,
  IN,
};
using rcomp_t = uint64_t;

class mr_impl_t;
class mr_t
{
 public:
  // attribute getter
  void* get_attr_user_context() const;
  mr_impl_t* p_impl = nullptr;

  mr_t() = default;
  mr_t(void* p) : p_impl(static_cast<mr_impl_t*>(p)) {}
  inline bool is_empty() const { return p_impl == nullptr; }
  inline mr_impl_t* get_impl() const
  {
    if (!p_impl) throw std::runtime_error("p_impl is nullptr!");
    return p_impl;
  }
  inline void set_impl(mr_impl_t* p) { p_impl = p; }
};

struct buffer_t {
  void* base;
  size_t size;
  mr_t mr;
  buffer_t() : base(nullptr), size(0), mr() {}
  buffer_t(void* base_, size_t size_) : base(base_), size(size_) {}
  buffer_t(void* base_, size_t size_, mr_t mr_)
      : base(base_), size(size_), mr(mr_)
  {
  }
};
struct rbuffer_t {
  uintptr_t base;
  size_t size;
  rkey_t rkey;
  rbuffer_t() : base(0), size(0), rkey(0) {}
  rbuffer_t(uintptr_t base_, size_t size_) : base(base_), size(size_) {}
  rbuffer_t(uintptr_t base_, size_t size_, rkey_t rkey_)
      : base(base_), size(size_), rkey(rkey_)
  {
  }
};
using buffers_t = std::vector<buffer_t>;
using rbuffers_t = std::vector<rbuffer_t>;
// A generic data type that can be a scalar, buffer, or buffers
// Make it within 24 bytes
struct data_t {
  static const int MAX_SCALAR_SIZE = 23;
  enum class type_t : unsigned int {
    none,
    scalar,
    buffer,
    buffers,
  };
  union {
    struct {
      // common fields
      type_t type : 2;
      bool own_data : 1;
    };
    struct {
      type_t type : 2;
      bool own_data : 1;
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
      size_t count : 61;
      buffer_t* buffers;
    } buffers;
  };
  data_t() : type(type_t::none), own_data(false) {}
  data_t(buffer_t buffer_, bool own_data_ = false)
  {
    type = type_t::buffer;
    own_data = own_data_;
    buffer.base = buffer_.base;
    buffer.size = buffer_.size;
    buffer.mr = buffer_.mr;
  }
  data_t(buffers_t buffers_, bool own_data_ = false)
  {
    type = type_t::buffers;
    own_data = own_data_;
    buffers.count = buffers_.size();
    buffers.buffers = new buffer_t[buffers.count];
    for (size_t i = 0; i < buffers.count; i++) {
      buffers.buffers[i] = buffers_[i];
    }
  }
  data_t(size_t size)
  {
    type = type_t::buffer;
    own_data = true;
    buffer.base = malloc(size);
    buffer.size = size;
    buffer.mr = mr_t();
  }
  data_t(size_t sizes[], int count)
  {
    type = type_t::buffers;
    own_data = true;
    buffers.count = count;
    buffers.buffers = new buffer_t[buffers.count];
    for (int i = 0; i < count; i++) {
      buffers.buffers[i].base = malloc(sizes[i]);
      buffers.buffers[i].size = sizes[i];
      buffers.buffers[i].mr = mr_t();
    }
  }
  // copy constructor
  data_t(const data_t& other)
  {
    type = other.type;
    own_data = other.own_data;
    if (own_data)
      fprintf(stderr, "Copying buffer with own_data=true is not recommended\n");
    if (other.is_scalar()) {
      memcpy(scalar.data, other.scalar.data, MAX_SCALAR_SIZE);
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
  data_t(data_t&& other)
  {
    type = other.type;
    own_data = other.own_data;
    if (other.is_scalar()) {
      memcpy(scalar.data, other.scalar.data, MAX_SCALAR_SIZE);
    } else if (other.is_buffer()) {
      buffer = other.buffer;
      other.own_data = false;
    } else if (other.is_buffers()) {
      buffers.count = other.buffers.count;
      buffers.buffers = other.buffers.buffers;
      other.own_data = false;
      other.buffers.buffers = nullptr;
    }
  }
  // copy assignment
  data_t& operator=(const data_t& other)
  {
    if (this == &other) {
      return *this;
    }
    if (own_data) {
      if (is_buffer()) {
        free(buffer.base);
      } else if (is_buffers()) {
        for (size_t i = 0; i < buffers.count; i++) {
          free(buffers.buffers[i].base);
        }
        delete[] buffers.buffers;
      }
    }
    type = other.type;
    own_data = other.own_data;
    if (own_data)
      fprintf(stderr, "Copying buffer with own_data=true is not recommended\n");
    if (other.is_scalar()) {
      memcpy(scalar.data, other.scalar.data, MAX_SCALAR_SIZE);
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
    return *this;
  }
  // move assignment
  data_t& operator=(data_t&& other)
  {
    if (this == &other) {
      return *this;
    }
    if (own_data) {
      if (is_buffer()) {
        free(buffer.base);
      } else if (is_buffers()) {
        for (size_t i = 0; i < buffers.count; i++) {
          free(buffers.buffers[i].base);
        }
        delete[] buffers.buffers;
      }
    }
    type = other.type;
    own_data = other.own_data;
    if (other.is_scalar()) {
      memcpy(scalar.data, other.scalar.data, MAX_SCALAR_SIZE);
    } else if (other.is_buffer()) {
      buffer = other.buffer;
      other.own_data = false;
    } else if (other.is_buffers()) {
      buffers.count = other.buffers.count;
      buffers.buffers = other.buffers.buffers;
      other.own_data = false;
      other.buffers.buffers = nullptr;
    }
    return *this;
  }
  ~data_t()
  {
    if (own_data) {
      if (is_buffer()) {
        free(buffer.base);
      } else if (is_buffers()) {
        for (size_t i = 0; i < buffers.count; i++) {
          free(buffers.buffers[i].base);
        }
      }
    }
    if (is_buffers()) {
      delete[] buffers.buffers;
    }
  }
  bool is_scalar() const { return type == type_t::scalar; }
  bool is_buffer() const { return type == type_t::buffer; }
  bool is_buffers() const { return type == type_t::buffers; }
  type_t get_type() const { return type; }
  void copy_from(const void* data_, size_t size)
  {
    if (size <= MAX_SCALAR_SIZE) {
      type = type_t::scalar;
      own_data = true;
      memcpy(scalar.data, data_, size);
    } else {
      type = type_t::buffer;
      own_data = true;
      buffer.base = malloc(size);
      memcpy(buffer.base, data_, size);
      buffer.size = size;
    }
  }
  void copy_from(const buffers_t& buffers_)
  {
    type = type_t::buffers;
    own_data = true;
    buffers.count = buffers_.size();
    buffers.buffers = new buffer_t[buffers.count];
    for (size_t i = 0; i < buffers.count; i++) {
      buffers.buffers[i].size = buffers_[i].size;
      buffers.buffers[i].base = malloc(buffers_[i].size);
      memcpy(buffers.buffers[i].base, buffers_[i].base, buffers_[i].size);
    }
  }
  enum class get_semantic_t {
    move,    // need to free the returned buffer; can only call get once.
    copy,    // need to free the returned buffer; can call get multiple times.
    borrow,  // no need to free the returned buffer; can call get multiple
             // times.
  };
  // the default is move semantic
  template <typename T>
  T get_scalar(/* always copy semantic */) const
  {
    // We still keep the ownership of the data
    if (is_buffer()) {
      if (buffer.size != sizeof(T)) {
        throw std::logic_error("Buffer size does not match scalar size");
      }
      return *reinterpret_cast<const T*>(buffer.base);
    } else if (is_scalar()) {
      static_assert(sizeof(T) <= 23, "Scalar size is too large");
      return *reinterpret_cast<const T*>(scalar.data);
    } else {
      throw std::logic_error("Cannot convert multiple buffers to a scalar");
    }
  }
  buffer_t get_buffer(get_semantic_t semantic = get_semantic_t::move)
  {
    buffer_t ret;
    if (is_scalar()) {
      if (semantic == get_semantic_t::move ||
          semantic == get_semantic_t::copy) {
        ret.size = MAX_SCALAR_SIZE;
        ret.base = malloc(MAX_SCALAR_SIZE);
        memcpy(ret.base, scalar.data, MAX_SCALAR_SIZE);
      } else {
        ret = buffer_t(scalar.data, MAX_SCALAR_SIZE);
      }
    } else if (is_buffer()) {
      if (semantic == get_semantic_t::copy && own_data) {
        ret.size = buffer.size;
        ret.base = malloc(buffer.size);
        memcpy(ret.base, buffer.base, buffer.size);
      } else {
        ret = buffer_t(buffer.base, buffer.size, buffer.mr);
      }
    } else {
      throw std::logic_error("Cannot convert multiple buffers to a buffer");
    }
    if (semantic == get_semantic_t::move) {
      own_data = false;
      type = type_t::none;
    }
    return ret;
  }
  size_t get_buffers_count() const
  {
    if (!is_buffers()) {
      throw std::logic_error("Not a buffers");
    }
    return buffers.count;
  }
  buffers_t get_buffers(get_semantic_t semantic = get_semantic_t::move)
  {
    if (!is_buffers()) {
      throw std::logic_error("Not a buffers");
    }
    if (semantic == get_semantic_t::move) {
      own_data = false;
      type = type_t::none;
    }
    buffers_t ret;
    for (size_t i = 0; i < buffers.count; i++) {
      if (semantic == get_semantic_t::copy && own_data) {
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
};

struct status_t {
  error_t error = errorcode_t::retry_init;
  int rank = -1;
  data_t data;
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