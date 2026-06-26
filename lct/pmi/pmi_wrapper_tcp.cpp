#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "pmi_wrapper.hpp"

// The "torchrun" backend provides a small TCP rendezvous service for launchers
// that do not provide PMI/PMIx. Select it with LCT_PMI_BACKEND=torchrun.
// Rank/world size come from RANK/WORLD_SIZE. LOCAL_RANK/LOCAL_WORLD_SIZE are
// parsed for diagnostics so torchrun local-rank metadata is not silently lost.
// The rendezvous endpoint is selected, in order, from:
//   LCT_MASTER_ADDR/LCT_MASTER_PORT, LCI_MASTER_ADDR/LCI_MASTER_PORT,
//   or torchrun's MASTER_ADDR/MASTER_PORT.
// MASTER_ADDR/MASTER_PORT is a compatibility fallback only: if torchrun (or
// another runtime) already owns MASTER_PORT, set LCI_MASTER_PORT or
// LCT_MASTER_PORT to a distinct port for LCI's TCP rendezvous.
// LCT_PMI_TCP_TIMEOUT_SEC controls connect/join/barrier timeouts (default 60s).

namespace lct
{
namespace pmi
{
namespace torchrun
{
namespace
{
using clock_t = std::chrono::steady_clock;
using deadline_t = clock_t::time_point;

constexpr int k_default_timeout_sec = 60;
constexpr size_t k_max_wire_string = 4096;

enum msg_type_t : uint32_t {
  MSG_HELLO = 1,
  MSG_PUBLISH = 2,
  MSG_GET = 3,
  MSG_BARRIER = 4,
  MSG_FINALIZE = 5,
  MSG_OK = 100,
  MSG_VALUE = 101,
  MSG_ERROR = 102,
};

struct wire_msg_t {
  msg_type_t type;
  int rank;
  uint32_t aux;
  std::string key;
  std::string value;
};

struct client_conn_t {
  int fd;
  int rank;
};

struct endpoint_spec_t {
  const char* source;
  const char* addr_name;
  const char* port_name;
};

struct endpoint_choice_t {
  const endpoint_spec_t* spec;
  const char* addr;
  const char* port;
};

constexpr endpoint_spec_t k_endpoint_specs[] = {
    {"LCT", "LCT_MASTER_ADDR", "LCT_MASTER_PORT"},
    {"LCI", "LCI_MASTER_ADDR", "LCI_MASTER_PORT"},
    {"MASTER", "MASTER_ADDR", "MASTER_PORT"},
};

int g_rank = -1;
int g_size = -1;
int g_local_rank = -1;
int g_local_size = -1;
int g_timeout_sec = k_default_timeout_sec;
std::string g_master_addr;
int g_master_port = -1;
std::string g_endpoint_source;
int g_client_fd = -1;
std::thread g_server_thread;
std::mutex g_server_mutex;
std::condition_variable g_server_cv;
bool g_server_ready = false;
bool g_server_done = false;
std::string g_server_error;
std::atomic<bool> g_server_has_error(false);

std::mutex g_barrier_mutex;
std::condition_variable g_barrier_cv;
int g_barrier_generation = 0;
int g_barrier_count = 0;

std::mutex g_kvs_mutex;
std::map<std::pair<int, std::string>, std::string> g_kvs;

std::string errno_string(const std::string& what)
{
  return what + ": " + strerror(errno) + " (errno " + std::to_string(errno) +
         ")";
}

[[noreturn]] void throw_errno(const std::string& what)
{
  throw std::runtime_error(errno_string(what));
}

[[noreturn]] void fail(const std::string& what)
{
  LCT_Assert(LCT_log_ctx_default, false, "%s", what.c_str());
  abort();
}

void fail_if(bool condition, const std::string& what)
{
  if (condition) fail(what);
}

int parse_int(const char* name, const char* value, int min_value,
              int max_value)
{
  fail_if(value == nullptr || *value == '\0',
          std::string("Missing required environment variable ") + name);
  errno = 0;
  char* end = nullptr;
  long parsed = strtol(value, &end, 10);
  fail_if(errno != 0 || end == value || *end != '\0' || parsed < min_value ||
              parsed > max_value,
          std::string("Invalid integer for ") + name + "='" + value +
              "' (expected " + std::to_string(min_value) + ".." +
              std::to_string(max_value) + ")");
  return static_cast<int>(parsed);
}

int parse_optional_int(const char* name, int default_value, int min_value,
                       int max_value)
{
  const char* value = getenv(name);
  if (value == nullptr) return default_value;
  return parse_int(name, value, min_value, max_value);
}

bool parse_int_no_fail(const char* value, int min_value, int max_value,
                       int* result = nullptr)
{
  if (value == nullptr || *value == '\0') return false;
  errno = 0;
  char* end = nullptr;
  long parsed = strtol(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed < min_value ||
      parsed > max_value)
    return false;
  if (result != nullptr) *result = static_cast<int>(parsed);
  return true;
}

bool has_env_value(const char* name)
{
  const char* value = getenv(name);
  return value != nullptr && value[0] != '\0';
}

bool select_endpoint(endpoint_choice_t* choice)
{
  for (const auto& spec : k_endpoint_specs) {
    const char* addr = getenv(spec.addr_name);
    const char* port = getenv(spec.port_name);
    if (addr != nullptr && addr[0] != '\0' && port != nullptr &&
        port[0] != '\0') {
      if (!parse_int_no_fail(port, 1, 65535)) return false;
      if (choice != nullptr) {
        choice->spec = &spec;
        choice->addr = addr;
        choice->port = port;
      }
      return true;
    }
  }
  return false;
}

std::string endpoint_help()
{
  return "Set a matched TCP rendezvous endpoint pair: "
         "LCT_MASTER_ADDR/LCT_MASTER_PORT (preferred), "
         "LCI_MASTER_ADDR/LCI_MASTER_PORT, or MASTER_ADDR/MASTER_PORT. "
         "Endpoint variables are not mixed across prefixes; for example, "
         "LCT_MASTER_ADDR with MASTER_PORT is intentionally ignored. "
         "MASTER_ADDR/MASTER_PORT is a compatibility fallback and may collide "
         "with torchrun's own rendezvous server, so prefer LCI_MASTER_PORT or "
         "LCT_MASTER_PORT when using torchrun.";
}

bool rank_size_env_is_valid()
{
  int rank = -1;
  int size = -1;
  if (!parse_int_no_fail(getenv("RANK"), 0, INT32_MAX - 1, &rank)) return false;
  if (!parse_int_no_fail(getenv("WORLD_SIZE"), 1, INT32_MAX - 1, &size))
    return false;
  return rank < size;
}

bool env_requests_only_torchrun_backend()
{
  const char* backend_env = getenv("LCT_PMI_BACKEND");
  if (backend_env == nullptr) return false;
  char* copy = strdup(backend_env);
  fail_if(copy == nullptr, "strdup failed while parsing LCT_PMI_BACKEND");
  bool saw_backend = false;
  bool only_torchrun = true;
  char* word = nullptr;
  char* rest = copy;
  while ((word = strtok_r(rest, " ;,", &rest))) {
    saw_backend = true;
    if (strcmp(word, "torchrun") != 0 && strcmp(word, "tcp") != 0) {
      only_torchrun = false;
      break;
    }
  }
  free(copy);
  return saw_backend && only_torchrun;
}

int deadline_ms_remaining(deadline_t deadline)
{
  auto now = clock_t::now();
  if (now >= deadline) return 0;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
                .count();
  if (ms > INT32_MAX) return INT32_MAX;
  return static_cast<int>(ms);
}

void set_cloexec(int fd)
{
  int flags = fcntl(fd, F_GETFD, 0);
  if (flags >= 0) fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

void set_nonblocking(int fd, bool nonblocking)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) throw_errno("fcntl(F_GETFL) failed");
  if (nonblocking)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) throw_errno("fcntl(F_SETFL) failed");
}

void close_fd(int* fd)
{
  if (*fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

bool wait_fd(int fd, short events, deadline_t deadline, const char* opname,
             std::string* error)
{
  while (true) {
    int timeout_ms = deadline_ms_remaining(deadline);
    if (timeout_ms <= 0) {
      *error = std::string(opname) + " timed out after " +
               std::to_string(g_timeout_sec) + " seconds";
      return false;
    }
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret > 0) return true;
    if (ret == 0) {
      *error = std::string(opname) + " timed out after " +
               std::to_string(g_timeout_sec) + " seconds";
      return false;
    }
    if (errno == EINTR) continue;
    *error = errno_string(std::string(opname) + " poll failed");
    return false;
  }
}

bool wait_fd_blocking(int fd, short events, const char* opname,
                      std::string* error)
{
  while (true) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    int ret = poll(&pfd, 1, -1);
    if (ret > 0) {
      if (pfd.revents & POLLNVAL) {
        *error = std::string(opname) + " failed: invalid file descriptor";
        return false;
      }
      return true;
    }
    if (errno == EINTR) continue;
    *error = errno_string(std::string(opname) + " poll failed");
    return false;
  }
}

bool write_all(int fd, const void* data, size_t size, deadline_t deadline,
               std::string* error)
{
  const char* p = static_cast<const char*>(data);
  size_t done = 0;
  while (done < size) {
    if (!wait_fd(fd, POLLOUT, deadline, "socket write", error)) return false;
#ifdef MSG_NOSIGNAL
    ssize_t ret = send(fd, p + done, size - done, MSG_NOSIGNAL);
#else
    ssize_t ret = write(fd, p + done, size - done);
#endif
    if (ret > 0) {
      done += static_cast<size_t>(ret);
      continue;
    }
    if (ret < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
      continue;
    *error = ret == 0 ? "socket write returned 0"
                      : errno_string("socket write failed");
    return false;
  }
  return true;
}

bool read_all(int fd, void* data, size_t size, deadline_t deadline,
              std::string* error)
{
  char* p = static_cast<char*>(data);
  size_t done = 0;
  while (done < size) {
    if (!wait_fd(fd, POLLIN, deadline, "socket read", error)) return false;
    ssize_t ret = read(fd, p + done, size - done);
    if (ret > 0) {
      done += static_cast<size_t>(ret);
      continue;
    }
    if (ret < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
      continue;
    *error = ret == 0 ? "peer closed TCP rendezvous connection"
                      : errno_string("socket read failed");
    return false;
  }
  return true;
}

bool send_msg(int fd, const wire_msg_t& msg, deadline_t deadline,
              std::string* error)
{
  fail_if(msg.key.size() > k_max_wire_string || msg.value.size() > k_max_wire_string,
          "LCT torchrun PMI wire message is too large");
  uint32_t header[5];
  header[0] = htonl(static_cast<uint32_t>(msg.type));
  header[1] = htonl(static_cast<uint32_t>(msg.rank));
  header[2] = htonl(msg.aux);
  header[3] = htonl(static_cast<uint32_t>(msg.key.size()));
  header[4] = htonl(static_cast<uint32_t>(msg.value.size()));
  if (!write_all(fd, header, sizeof(header), deadline, error)) return false;
  if (!msg.key.empty() &&
      !write_all(fd, msg.key.data(), msg.key.size(), deadline, error))
    return false;
  if (!msg.value.empty() &&
      !write_all(fd, msg.value.data(), msg.value.size(), deadline, error))
    return false;
  return true;
}

bool recv_msg(int fd, wire_msg_t* msg, deadline_t deadline, std::string* error)
{
  uint32_t header[5];
  if (!read_all(fd, header, sizeof(header), deadline, error)) return false;
  uint32_t type = ntohl(header[0]);
  uint32_t rank = ntohl(header[1]);
  uint32_t aux = ntohl(header[2]);
  uint32_t key_len = ntohl(header[3]);
  uint32_t value_len = ntohl(header[4]);
  if (key_len > k_max_wire_string || value_len > k_max_wire_string) {
    *error = "TCP rendezvous peer sent an oversized message";
    return false;
  }
  msg->type = static_cast<msg_type_t>(type);
  msg->rank = static_cast<int>(rank);
  msg->aux = aux;
  msg->key.assign(key_len, '\0');
  msg->value.assign(value_len, '\0');
  if (key_len > 0 &&
      !read_all(fd, &msg->key[0], key_len, deadline, error))
    return false;
  if (value_len > 0 &&
      !read_all(fd, &msg->value[0], value_len, deadline, error))
    return false;
  return true;
}

bool send_response(int fd, msg_type_t type, const std::string& payload,
                   deadline_t deadline, std::string* error)
{
  wire_msg_t msg;
  msg.type = type;
  msg.rank = 0;
  msg.aux = 0;
  msg.value = payload;
  return send_msg(fd, msg, deadline, error);
}

wire_msg_t request_response(const wire_msg_t& request)
{
  deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
  std::string error;
  if (!send_msg(g_client_fd, request, deadline, &error)) {
    fail("LCT torchrun PMI failed to send request to TCP rendezvous server: " +
         error);
  }
  wire_msg_t response;
  if (!recv_msg(g_client_fd, &response, deadline, &error)) {
    fail("LCT torchrun PMI failed waiting for TCP rendezvous server response: " +
         error);
  }
  if (response.type == MSG_ERROR) {
    fail("LCT torchrun PMI server error: " + response.value);
  }
  if (response.type != MSG_OK && response.type != MSG_VALUE) {
    fail("LCT torchrun PMI received unexpected response type " +
         std::to_string(static_cast<uint32_t>(response.type)));
  }
  return response;
}

void mark_server_error(const std::string& error)
{
  {
    std::lock_guard<std::mutex> lock(g_server_mutex);
    if (g_server_error.empty()) {
      g_server_error = error;
      g_server_has_error.store(true, std::memory_order_release);
    }
  }
  g_server_cv.notify_all();
  g_barrier_cv.notify_all();
}

std::string get_server_error()
{
  std::lock_guard<std::mutex> lock(g_server_mutex);
  return g_server_error;
}

void mark_server_ready()
{
  {
    std::lock_guard<std::mutex> lock(g_server_mutex);
    g_server_ready = true;
  }
  g_server_cv.notify_all();
}

void mark_server_done()
{
  {
    std::lock_guard<std::mutex> lock(g_server_mutex);
    g_server_done = true;
  }
  g_server_cv.notify_all();
}

int create_listener(int port)
{
  int fd = socket(AF_INET6, SOCK_STREAM, 0);
  int family = AF_INET6;
  if (fd < 0) {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    family = AF_INET;
  }
  if (fd < 0) throw_errno("socket() for TCP rendezvous listener failed");
  set_cloexec(fd);
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (family == AF_INET6) {
    int zero = 0;
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(zero));
    struct sockaddr_in6 addr6;
    memset(&addr6, 0, sizeof(addr6));
    addr6.sin6_family = AF_INET6;
    addr6.sin6_addr = in6addr_any;
    addr6.sin6_port = htons(static_cast<uint16_t>(port));
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr6), sizeof(addr6)) <
        0) {
      int saved_errno = errno;
      close(fd);
      errno = saved_errno;
      throw_errno("bind() for LCT torchrun TCP rendezvous listener failed at " +
                  g_master_addr + ":" + std::to_string(port) +
                  (g_endpoint_source == "MASTER"
                       ? "; MASTER_PORT may already be in use by torchrun. Set "
                         "LCI_MASTER_PORT or LCT_MASTER_PORT to a distinct "
                         "free port"
                       : ""));
    }
  } else {
    struct sockaddr_in addr4;
    memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_addr.s_addr = htonl(INADDR_ANY);
    addr4.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr4), sizeof(addr4)) <
        0) {
      int saved_errno = errno;
      close(fd);
      errno = saved_errno;
      throw_errno("bind() for LCT torchrun TCP rendezvous listener failed at " +
                  g_master_addr + ":" + std::to_string(port) +
                  (g_endpoint_source == "MASTER"
                       ? "; MASTER_PORT may already be in use by torchrun. Set "
                         "LCI_MASTER_PORT or LCT_MASTER_PORT to a distinct "
                         "free port"
                       : ""));
    }
  }
  if (listen(fd, std::max(8, g_size)) < 0) {
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    throw_errno("listen() for TCP rendezvous listener failed");
  }
  return fd;
}

int accept_with_timeout(int listen_fd, deadline_t deadline)
{
  std::string error;
  if (!wait_fd(listen_fd, POLLIN, deadline, "waiting for torchrun ranks to join",
               &error)) {
    throw std::runtime_error("Timed out waiting for all " +
                             std::to_string(g_size) +
                             " ranks to join TCP rendezvous at " +
                             g_master_addr + ":" +
                             std::to_string(g_master_port) + " (" + error +
                             ")");
  }
  int fd = accept(listen_fd, nullptr, nullptr);
  if (fd < 0) throw_errno("accept() on TCP rendezvous listener failed");
  set_cloexec(fd);
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  return fd;
}

int connect_to_server(deadline_t deadline)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  std::string port_str = std::to_string(g_master_port);
  struct addrinfo* addrs = nullptr;
  int gai = getaddrinfo(g_master_addr.c_str(), port_str.c_str(), &hints, &addrs);
  if (gai != 0) {
    fail("LCT torchrun PMI could not resolve rendezvous endpoint " +
         g_master_addr + ":" + port_str + ": " + gai_strerror(gai));
  }

  std::string last_error;
  while (clock_t::now() < deadline) {
    for (struct addrinfo* ai = addrs; ai != nullptr; ai = ai->ai_next) {
      int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd < 0) {
        last_error = errno_string("socket() failed");
        continue;
      }
      set_cloexec(fd);
      int one = 1;
      setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
      try {
        set_nonblocking(fd, true);
      } catch (const std::exception& e) {
        last_error = e.what();
        close(fd);
        continue;
      }
      int ret = connect(fd, ai->ai_addr, ai->ai_addrlen);
      if (ret == 0) {
        set_nonblocking(fd, false);
        freeaddrinfo(addrs);
        return fd;
      }
      if (errno == EINPROGRESS) {
        std::string error;
        if (wait_fd(fd, POLLOUT, deadline, "connecting to TCP rendezvous server",
                    &error)) {
          int so_error = 0;
          socklen_t len = sizeof(so_error);
          if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) == 0 &&
              so_error == 0) {
            set_nonblocking(fd, false);
            freeaddrinfo(addrs);
            return fd;
          }
          if (so_error != 0) {
            errno = so_error;
            last_error = errno_string("connect() failed");
          } else {
            last_error = errno_string("getsockopt(SO_ERROR) failed");
          }
        } else {
          last_error = error;
        }
      } else {
        last_error = errno_string("connect() failed");
      }
      close(fd);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  freeaddrinfo(addrs);
  fail("LCT torchrun PMI rank " + std::to_string(g_rank) + " timed out after " +
       std::to_string(g_timeout_sec) + " seconds connecting to TCP rendezvous " +
       "server at " + g_master_addr + ":" + port_str +
       (last_error.empty() ? std::string() : " (last error: " + last_error + ")"));
}

void handle_barrier(int fd, int rank)
{
  deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
  int generation = 0;
  {
    std::unique_lock<std::mutex> lock(g_barrier_mutex);
    generation = g_barrier_generation;
    ++g_barrier_count;
    LCT_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "pmi_torchrun",
            "Rank %d reached TCP barrier generation %d (%d/%d).\n", rank,
            generation, g_barrier_count, g_size);
    if (g_barrier_count == g_size) {
      g_barrier_count = 0;
      ++g_barrier_generation;
      g_barrier_cv.notify_all();
    } else {
      bool ok = g_barrier_cv.wait_until(lock, deadline, [&] {
        return g_barrier_generation != generation ||
               g_server_has_error.load(std::memory_order_acquire);
      });
      if (!ok && g_barrier_generation == generation) {
        std::string error = "Timed out after " + std::to_string(g_timeout_sec) +
                            " seconds in LCT torchrun TCP barrier generation " +
                            std::to_string(generation) + " (rank " +
                            std::to_string(rank) + ", " +
                            std::to_string(g_barrier_count) + "/" +
                            std::to_string(g_size) + " ranks arrived)";
        mark_server_error(error);
      }
      if (g_server_has_error.load(std::memory_order_acquire)) {
        std::string error = get_server_error();
        lock.unlock();
        std::string send_error;
        send_response(fd, MSG_ERROR, error, deadline, &send_error);
        return;
      }
    }
  }
  std::string error;
  send_response(fd, MSG_OK, "", deadline, &error);
}

void client_handler(int fd, int rank)
{
  while (true) {
    wire_msg_t msg;
    std::string error;
    if (!wait_fd_blocking(fd, POLLIN, "waiting for next LCT torchrun PMI request",
                          &error)) {
      close(fd);
      return;
    }
    deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
    if (!recv_msg(fd, &msg, deadline, &error)) {
      close(fd);
      return;
    }
    deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
    if (msg.type == MSG_PUBLISH) {
      {
        std::lock_guard<std::mutex> lock(g_kvs_mutex);
        g_kvs[std::make_pair(rank, msg.key)] = msg.value;
      }
      send_response(fd, MSG_OK, "", deadline, &error);
    } else if (msg.type == MSG_GET) {
      if (msg.rank < 0 || msg.rank >= g_size) {
        send_response(fd, MSG_ERROR,
                      "LCT torchrun PMI getname requested invalid rank " +
                          std::to_string(msg.rank) + " for key '" + msg.key +
                          "'",
                      deadline, &error);
        continue;
      }
      std::string value;
      bool found = false;
      {
        std::lock_guard<std::mutex> lock(g_kvs_mutex);
        auto it = g_kvs.find(std::make_pair(msg.rank, msg.key));
        if (it != g_kvs.end()) {
          value = it->second;
          found = true;
        }
      }
      if (found) {
        send_response(fd, MSG_VALUE, value, deadline, &error);
      } else {
        send_response(fd, MSG_ERROR,
                      "LCT torchrun PMI key '" + msg.key +
                          "' from rank " + std::to_string(msg.rank) +
                          " was not found. Ensure all ranks call "
                          "LCT_pmi_publish before LCT_pmi_barrier.",
                      deadline, &error);
      }
    } else if (msg.type == MSG_BARRIER) {
      handle_barrier(fd, rank);
    } else if (msg.type == MSG_FINALIZE) {
      send_response(fd, MSG_OK, "", deadline, &error);
      close(fd);
      return;
    } else {
      send_response(fd, MSG_ERROR,
                    "LCT torchrun PMI server received unexpected request type " +
                        std::to_string(static_cast<uint32_t>(msg.type)),
                    deadline, &error);
    }
  }
}

void server_loop()
{
  int listen_fd = create_listener(g_master_port);
  mark_server_ready();
  deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
  std::vector<client_conn_t> clients;
  std::vector<bool> seen_rank(g_size, false);
  try {
    while (static_cast<int>(clients.size()) < g_size) {
      int fd = accept_with_timeout(listen_fd, deadline);
      wire_msg_t hello;
      std::string error;
      if (!recv_msg(fd, &hello, deadline, &error)) {
        close(fd);
        throw std::runtime_error("Failed to read hello from TCP rendezvous "
                                 "client: " +
                                 error);
      }
      if (hello.type != MSG_HELLO || hello.rank < 0 || hello.rank >= g_size) {
        send_response(fd, MSG_ERROR,
                      "Invalid LCT torchrun PMI hello from rank " +
                          std::to_string(hello.rank),
                      deadline, &error);
        close(fd);
        throw std::runtime_error("Invalid TCP rendezvous hello");
      }
      if (seen_rank[hello.rank]) {
        send_response(fd, MSG_ERROR,
                      "Duplicate LCT torchrun PMI connection for rank " +
                          std::to_string(hello.rank),
                      deadline, &error);
        close(fd);
        throw std::runtime_error("Duplicate TCP rendezvous rank " +
                                 std::to_string(hello.rank));
      }
      seen_rank[hello.rank] = true;
      clients.push_back({fd, hello.rank});
    }
    for (auto& client : clients) {
      std::string error;
      if (!send_response(client.fd, MSG_OK, "", deadline, &error)) {
        throw std::runtime_error("Failed to send TCP rendezvous welcome to rank " +
                                 std::to_string(client.rank) + ": " + error);
      }
    }
  } catch (const std::exception& e) {
    for (auto& client : clients) {
      std::string error;
      send_response(client.fd, MSG_ERROR, e.what(), deadline, &error);
      close(client.fd);
    }
    close(listen_fd);
    throw;
  }
  close(listen_fd);

  std::vector<std::thread> handlers;
  handlers.reserve(clients.size());
  for (auto& client : clients) {
    handlers.emplace_back(client_handler, client.fd, client.rank);
  }
  for (auto& handler : handlers) handler.join();
}

void server_main()
{
  try {
    server_loop();
  } catch (const std::exception& e) {
    mark_server_error(e.what());
  }
  mark_server_done();
}

void parse_environment()
{
  g_rank = parse_int("RANK", getenv("RANK"), 0, INT32_MAX - 1);
  g_size = parse_int("WORLD_SIZE", getenv("WORLD_SIZE"), 1, INT32_MAX - 1);
  fail_if(g_rank >= g_size,
          "Invalid torchrun rank: RANK=" + std::to_string(g_rank) +
              " must be smaller than WORLD_SIZE=" + std::to_string(g_size));
  g_local_rank = parse_optional_int("LOCAL_RANK", -1, 0, INT32_MAX - 1);
  g_local_size = parse_optional_int("LOCAL_WORLD_SIZE", -1, 1, INT32_MAX - 1);
  if (g_local_rank >= 0 && g_local_size >= 0) {
    fail_if(g_local_rank >= g_local_size,
            "Invalid torchrun local rank: LOCAL_RANK=" +
                std::to_string(g_local_rank) +
                " must be smaller than LOCAL_WORLD_SIZE=" +
                std::to_string(g_local_size));
  }

  endpoint_choice_t endpoint = {};
  fail_if(!select_endpoint(&endpoint),
          "LCT torchrun PMI requires an explicit matched TCP rendezvous "
          "endpoint. " +
              endpoint_help());
  g_master_addr = endpoint.addr;
  g_master_port = parse_int(endpoint.spec->port_name, endpoint.port, 1, 65535);
  g_endpoint_source = endpoint.spec->source;
  g_timeout_sec = parse_optional_int("LCT_PMI_TCP_TIMEOUT_SEC",
                                     k_default_timeout_sec, 1, 24 * 60 * 60);
}

}  // namespace

int check_availability()
{
  bool have_rank_size = has_env_value("RANK") && has_env_value("WORLD_SIZE");
  if (!have_rank_size) return false;

  bool explicit_torchrun = env_requests_only_torchrun_backend();
  if (!rank_size_env_is_valid()) {
    fail_if(explicit_torchrun,
            "LCT_PMI_BACKEND=torchrun requires valid RANK and WORLD_SIZE "
            "environment variables");
    return false;
  }

  endpoint_choice_t endpoint = {};
  bool have_endpoint = select_endpoint(&endpoint);
  fail_if(explicit_torchrun && !have_endpoint,
          "LCT_PMI_BACKEND=torchrun requires a usable matched TCP rendezvous "
          "endpoint. " +
              endpoint_help());
  return have_endpoint;
}

void initialize()
{
  parse_environment();
  deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);

  if (g_rank == 0) {
    {
      std::lock_guard<std::mutex> kvs_lock(g_kvs_mutex);
      g_kvs.clear();
    }
    {
      std::lock_guard<std::mutex> barrier_lock(g_barrier_mutex);
      g_barrier_generation = 0;
      g_barrier_count = 0;
    }
    {
      std::lock_guard<std::mutex> lock(g_server_mutex);
      g_server_ready = false;
      g_server_done = false;
      g_server_error.clear();
      g_server_has_error.store(false, std::memory_order_release);
    }
    g_server_thread = std::thread(server_main);
    std::unique_lock<std::mutex> lock(g_server_mutex);
    bool ready = g_server_cv.wait_until(lock, deadline, [] {
      return g_server_ready || !g_server_error.empty();
    });
    fail_if(!ready,
            "LCT torchrun PMI rank 0 timed out starting TCP rendezvous server "
            "at port " +
                std::to_string(g_master_port));
    fail_if(!g_server_error.empty(),
            "LCT torchrun PMI failed to start TCP rendezvous server: " +
                g_server_error);
  }

  g_client_fd = connect_to_server(deadline);
  wire_msg_t hello;
  hello.type = MSG_HELLO;
  hello.rank = g_rank;
  std::string error;
  if (!send_msg(g_client_fd, hello, deadline, &error)) {
    fail("LCT torchrun PMI failed to send hello to TCP rendezvous server: " +
         error);
  }
  wire_msg_t response;
  if (!recv_msg(g_client_fd, &response, deadline, &error)) {
    fail("LCT torchrun PMI timed out waiting for all ranks to join at " +
         g_master_addr + ":" + std::to_string(g_master_port) + ": " + error);
  }
  if (response.type == MSG_ERROR) {
    fail("LCT torchrun PMI failed during rendezvous: " + response.value);
  }
  fail_if(response.type != MSG_OK,
          "LCT torchrun PMI received unexpected hello response type " +
              std::to_string(static_cast<uint32_t>(response.type)));

  LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi_torchrun",
          "Connected rank %d/%d to TCP rendezvous %s:%d (local rank %d/%d, "
          "timeout %ds).\n",
          g_rank, g_size, g_master_addr.c_str(), g_master_port, g_local_rank,
          g_local_size, g_timeout_sec);
}

int initialized() { return g_client_fd >= 0; }
int get_rank_me() { return g_rank; }
int get_size() { return g_size; }

void publish(char* key, char* value)
{
  fail_if(!initialized(), "LCT torchrun PMI publish called before initialize");
  fail_if(key == nullptr || value == nullptr,
          "LCT torchrun PMI publish received a null key/value");
  fail_if(strlen(key) >= LCT_PMI_STRING_LIMIT ||
              strlen(value) >= LCT_PMI_STRING_LIMIT,
          "LCT torchrun PMI publish key/value exceeds LCT_PMI_STRING_LIMIT");
  wire_msg_t msg;
  msg.type = MSG_PUBLISH;
  msg.rank = g_rank;
  msg.key = key;
  msg.value = value;
  wire_msg_t response = request_response(msg);
  fail_if(response.type != MSG_OK,
          "LCT torchrun PMI publish received unexpected response");
}

void getname(int rank, char* key, char* value)
{
  fail_if(!initialized(), "LCT torchrun PMI getname called before initialize");
  fail_if(key == nullptr || value == nullptr,
          "LCT torchrun PMI getname received a null key/value");
  fail_if(strlen(key) >= LCT_PMI_STRING_LIMIT,
          "LCT torchrun PMI getname key exceeds LCT_PMI_STRING_LIMIT");
  wire_msg_t msg;
  msg.type = MSG_GET;
  msg.rank = rank;
  msg.key = key;
  wire_msg_t response = request_response(msg);
  fail_if(response.type != MSG_VALUE,
          "LCT torchrun PMI getname received unexpected response");
  fail_if(response.value.size() >= LCT_PMI_STRING_LIMIT,
          "LCT torchrun PMI getname value exceeds LCT_PMI_STRING_LIMIT");
  strcpy(value, response.value.c_str());
}

void barrier()
{
  fail_if(!initialized(), "LCT torchrun PMI barrier called before initialize");
  wire_msg_t msg;
  msg.type = MSG_BARRIER;
  msg.rank = g_rank;
  wire_msg_t response = request_response(msg);
  fail_if(response.type != MSG_OK,
          "LCT torchrun PMI barrier received unexpected response");
}

void finalize()
{
  if (g_client_fd >= 0) {
    deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
    wire_msg_t msg;
    msg.type = MSG_FINALIZE;
    msg.rank = g_rank;
    std::string error;
    send_msg(g_client_fd, msg, deadline, &error);
    wire_msg_t response;
    recv_msg(g_client_fd, &response, deadline, &error);
    close_fd(&g_client_fd);
  }
  if (g_rank == 0 && g_server_thread.joinable()) {
    std::unique_lock<std::mutex> lock(g_server_mutex);
    g_server_cv.wait_for(lock, std::chrono::seconds(1), [] { return g_server_done; });
    bool done = g_server_done;
    lock.unlock();
    (void)done;
    g_server_thread.join();
  }
  g_rank = -1;
  g_size = -1;
  g_local_rank = -1;
  g_local_size = -1;
  g_master_addr.clear();
  g_master_port = -1;
  g_endpoint_source.clear();
  g_timeout_sec = k_default_timeout_sec;
}

}  // namespace torchrun

void torchrun_setup_ops(struct ops_t* ops)
{
  ops->check_availability = torchrun::check_availability;
  ops->initialize = torchrun::initialize;
  ops->is_initialized = torchrun::initialized;
  ops->get_rank_me = torchrun::get_rank_me;
  ops->get_size = torchrun::get_size;
  ops->publish = torchrun::publish;
  ops->getname = torchrun::getname;
  ops->barrier = torchrun::barrier;
  ops->finalize = torchrun::finalize;
}

}  // namespace pmi
}  // namespace lct
