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
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
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
//
// This backend intentionally follows the PMI put/fence/get shape without any
// background progress threads:
//   - publish() only records a local pending key/value.
//   - barrier() is the fence. Nonzero ranks send pending puts and a barrier
//     request; rank 0 polls all sockets and progresses the rendezvous server in
//     that calling thread; rank 0 then sends the post-fence KVS snapshot back.
//   - getname() is a local lookup in the most recent post-barrier snapshot.
// Keeping all TCP progress inside initialize(), barrier(), and finalize()
// prevents ordinary LCI application work from contending with hidden rendezvous
// server or per-rank handler threads.

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
std::vector<client_conn_t> g_clients;
std::map<std::string, std::string> g_pending_kvs;
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

int parse_int(const char* name, const char* value, int min_value, int max_value)
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
  auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
          .count();
  if (ms > INT32_MAX) return INT32_MAX;
  return static_cast<int>(ms);
}

void poll_sleep_ms(int ms)
{
  while (ms > 0) {
    int ret = poll(nullptr, 0, ms);
    if (ret == 0) return;
    if (ret < 0 && errno == EINTR) continue;
    return;
  }
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

void close_clients()
{
  for (auto& client : g_clients) close_fd(&client.fd);
  g_clients.clear();
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
    if (ret > 0) {
      if (pfd.revents & POLLNVAL) {
        *error = std::string(opname) + " failed: invalid file descriptor";
        return false;
      }
      return true;
    }
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
  fail_if(msg.key.size() > k_max_wire_string ||
              msg.value.size() > k_max_wire_string,
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
  if (key_len > 0 && !read_all(fd, &msg->key[0], key_len, deadline, error))
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
  if (!wait_fd(listen_fd, POLLIN, deadline,
               "waiting for torchrun ranks to join", &error)) {
    throw std::runtime_error(
        "Timed out waiting for all " + std::to_string(g_size) +
        " ranks to join TCP rendezvous at " + g_master_addr + ":" +
        std::to_string(g_master_port) + " (" + error + ")");
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
  int gai =
      getaddrinfo(g_master_addr.c_str(), port_str.c_str(), &hints, &addrs);
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
        if (wait_fd(fd, POLLOUT, deadline,
                    "connecting to TCP rendezvous server", &error)) {
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
    poll_sleep_ms(100);
  }
  freeaddrinfo(addrs);
  fail("LCT torchrun PMI rank " + std::to_string(g_rank) + " timed out after " +
       std::to_string(g_timeout_sec) +
       " seconds connecting to TCP rendezvous " + "server at " + g_master_addr +
       ":" + port_str +
       (last_error.empty() ? std::string()
                           : " (last error: " + last_error + ")"));
}

void send_error_to_clients(const std::string& message, deadline_t deadline)
{
  for (auto& client : g_clients) {
    std::string error;
    send_response(client.fd, MSG_ERROR, message, deadline, &error);
  }
}

void accept_rank_clients(int listen_fd, deadline_t deadline)
{
  std::vector<bool> seen_rank(g_size, false);
  seen_rank[0] = true;
  while (static_cast<int>(g_clients.size()) < g_size - 1) {
    int fd = accept_with_timeout(listen_fd, deadline);
    wire_msg_t hello;
    std::string error;
    if (!recv_msg(fd, &hello, deadline, &error)) {
      close(fd);
      throw std::runtime_error(
          "Failed to read hello from TCP rendezvous client: " + error);
    }
    if (hello.type != MSG_HELLO || hello.rank <= 0 || hello.rank >= g_size) {
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
    g_clients.push_back({fd, hello.rank});
  }
  for (auto& client : g_clients) {
    std::string error;
    if (!send_response(client.fd, MSG_OK, "", deadline, &error)) {
      throw std::runtime_error(
          "Failed to send TCP rendezvous welcome to rank " +
          std::to_string(client.rank) + ": " + error);
    }
  }
}

void apply_pending_publishes()
{
  for (const auto& kv : g_pending_kvs) {
    g_kvs[std::make_pair(g_rank, kv.first)] = kv.second;
  }
  g_pending_kvs.clear();
}

void store_remote_publish(const wire_msg_t& msg, int client_rank)
{
  fail_if(msg.rank != client_rank,
          "LCT torchrun PMI received publish with mismatched rank " +
              std::to_string(msg.rank) + " on connection for rank " +
              std::to_string(client_rank));
  fail_if(msg.key.size() >= LCT_PMI_STRING_LIMIT ||
              msg.value.size() >= LCT_PMI_STRING_LIMIT,
          "LCT torchrun PMI publish key/value exceeds LCT_PMI_STRING_LIMIT");
  g_kvs[std::make_pair(client_rank, msg.key)] = msg.value;
}

void send_snapshot(int fd, deadline_t deadline)
{
  fail_if(g_kvs.size() > UINT32_MAX,
          "LCT torchrun PMI KVS snapshot has too many entries");
  wire_msg_t ok;
  ok.type = MSG_OK;
  ok.rank = 0;
  ok.aux = static_cast<uint32_t>(g_kvs.size());
  std::string error;
  if (!send_msg(fd, ok, deadline, &error)) {
    fail("LCT torchrun PMI failed to send barrier response: " + error);
  }
  for (const auto& kv : g_kvs) {
    wire_msg_t record;
    record.type = MSG_VALUE;
    record.rank = kv.first.first;
    record.key = kv.first.second;
    record.value = kv.second;
    if (!send_msg(fd, record, deadline, &error)) {
      fail("LCT torchrun PMI failed to send KVS snapshot record to rank: " +
           error);
    }
  }
}

void recv_snapshot(uint32_t count, deadline_t deadline)
{
  std::map<std::pair<int, std::string>, std::string> snapshot;
  for (uint32_t i = 0; i < count; ++i) {
    wire_msg_t record;
    std::string error;
    if (!recv_msg(g_client_fd, &record, deadline, &error)) {
      fail("LCT torchrun PMI failed receiving KVS snapshot: " + error);
    }
    if (record.type == MSG_ERROR) {
      fail("LCT torchrun PMI server error: " + record.value);
    }
    fail_if(record.type != MSG_VALUE,
            "LCT torchrun PMI received unexpected KVS snapshot record type " +
                std::to_string(static_cast<uint32_t>(record.type)));
    fail_if(record.rank < 0 || record.rank >= g_size,
            "LCT torchrun PMI snapshot contains invalid rank " +
                std::to_string(record.rank));
    fail_if(record.key.size() >= LCT_PMI_STRING_LIMIT ||
                record.value.size() >= LCT_PMI_STRING_LIMIT,
            "LCT torchrun PMI getname value exceeds LCT_PMI_STRING_LIMIT");
    snapshot[std::make_pair(record.rank, record.key)] = record.value;
  }
  g_kvs.swap(snapshot);
}

void rank0_barrier()
{
  apply_pending_publishes();

  deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
  std::vector<bool> arrived(g_size, false);
  arrived[0] = true;
  int arrived_count = 1;

  while (arrived_count < g_size) {
    std::vector<struct pollfd> pfds;
    std::vector<int> client_indices;
    for (size_t i = 0; i < g_clients.size(); ++i) {
      const client_conn_t& client = g_clients[i];
      if (client.fd >= 0 && !arrived[client.rank]) {
        struct pollfd pfd;
        pfd.fd = client.fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        pfds.push_back(pfd);
        client_indices.push_back(static_cast<int>(i));
      }
    }

    int timeout_ms = deadline_ms_remaining(deadline);
    if (timeout_ms <= 0) {
      fail("Timed out after " + std::to_string(g_timeout_sec) +
           " seconds in LCT torchrun TCP barrier (" +
           std::to_string(arrived_count) + "/" + std::to_string(g_size) +
           " ranks arrived)");
    }
    int ret = poll(pfds.data(), pfds.size(), timeout_ms);
    if (ret == 0) {
      fail("Timed out after " + std::to_string(g_timeout_sec) +
           " seconds in LCT torchrun TCP barrier (" +
           std::to_string(arrived_count) + "/" + std::to_string(g_size) +
           " ranks arrived)");
    }
    if (ret < 0) {
      if (errno == EINTR) continue;
      fail(
          errno_string("poll failed while progressing LCT torchrun TCP "
                       "barrier"));
    }

    for (size_t i = 0; i < pfds.size(); ++i) {
      if (pfds[i].revents == 0) continue;
      client_conn_t& client = g_clients[client_indices[i]];
      if (pfds[i].revents & POLLNVAL) {
        fail("LCT torchrun PMI barrier saw invalid socket for rank " +
             std::to_string(client.rank));
      }
      if (pfds[i].revents & POLLERR) {
        fail("LCT torchrun PMI barrier saw socket error for rank " +
             std::to_string(client.rank));
      }
      if (!(pfds[i].revents & (POLLIN | POLLHUP))) continue;

      wire_msg_t msg;
      std::string error;
      if (!recv_msg(client.fd, &msg, deadline, &error)) {
        fail("LCT torchrun PMI failed receiving barrier message from rank " +
             std::to_string(client.rank) + ": " + error);
      }
      if (msg.type == MSG_PUBLISH) {
        store_remote_publish(msg, client.rank);
      } else if (msg.type == MSG_BARRIER) {
        fail_if(msg.rank != client.rank,
                "LCT torchrun PMI received barrier with mismatched rank " +
                    std::to_string(msg.rank) + " on connection for rank " +
                    std::to_string(client.rank));
        arrived[client.rank] = true;
        ++arrived_count;
        LCT_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "pmi_torchrun",
                "Rank %d reached TCP barrier (%d/%d).\n", client.rank,
                arrived_count, g_size);
      } else if (msg.type == MSG_FINALIZE) {
        fail("LCT torchrun PMI rank " + std::to_string(client.rank) +
             " finalized before reaching the active barrier");
      } else {
        send_response(client.fd, MSG_ERROR,
                      "LCT torchrun PMI server received unexpected request "
                      "type " +
                          std::to_string(static_cast<uint32_t>(msg.type)) +
                          " during barrier",
                      deadline, &error);
        fail("LCT torchrun PMI server received unexpected request type " +
             std::to_string(static_cast<uint32_t>(msg.type)) +
             " during barrier");
      }
    }
  }

  for (auto& client : g_clients) send_snapshot(client.fd, deadline);
}

void nonzero_barrier()
{
  deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
  std::string error;
  for (const auto& kv : g_pending_kvs) {
    wire_msg_t publish_msg;
    publish_msg.type = MSG_PUBLISH;
    publish_msg.rank = g_rank;
    publish_msg.key = kv.first;
    publish_msg.value = kv.second;
    if (!send_msg(g_client_fd, publish_msg, deadline, &error)) {
      fail("LCT torchrun PMI failed to send pending publish during barrier: " +
           error);
    }
  }

  wire_msg_t barrier_msg;
  barrier_msg.type = MSG_BARRIER;
  barrier_msg.rank = g_rank;
  if (!send_msg(g_client_fd, barrier_msg, deadline, &error)) {
    fail("LCT torchrun PMI failed to send barrier request: " + error);
  }

  wire_msg_t response;
  if (!recv_msg(g_client_fd, &response, deadline, &error)) {
    fail("LCT torchrun PMI failed waiting for TCP rendezvous barrier: " +
         error);
  }
  if (response.type == MSG_ERROR) {
    fail("LCT torchrun PMI server error: " + response.value);
  }
  fail_if(response.type != MSG_OK,
          "LCT torchrun PMI barrier received unexpected response type " +
              std::to_string(static_cast<uint32_t>(response.type)));
  recv_snapshot(response.aux, deadline);
  g_pending_kvs.clear();
}

void rank0_finalize()
{
  deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
  std::vector<bool> done(g_clients.size(), false);
  size_t done_count = 0;

  while (done_count < g_clients.size()) {
    std::vector<struct pollfd> pfds;
    std::vector<int> client_indices;
    for (size_t i = 0; i < g_clients.size(); ++i) {
      if (!done[i] && g_clients[i].fd >= 0) {
        struct pollfd pfd;
        pfd.fd = g_clients[i].fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        pfds.push_back(pfd);
        client_indices.push_back(static_cast<int>(i));
      }
    }

    int timeout_ms = deadline_ms_remaining(deadline);
    if (timeout_ms <= 0) {
      close_clients();
      fail("Timed out after " + std::to_string(g_timeout_sec) +
           " seconds waiting for LCT torchrun TCP ranks to finalize (" +
           std::to_string(done_count) + "/" + std::to_string(done.size()) +
           " clients finalized)");
    }
    int ret = poll(pfds.data(), pfds.size(), timeout_ms);
    if (ret == 0) {
      close_clients();
      fail("Timed out after " + std::to_string(g_timeout_sec) +
           " seconds waiting for LCT torchrun TCP ranks to finalize (" +
           std::to_string(done_count) + "/" + std::to_string(done.size()) +
           " clients finalized)");
    }
    if (ret < 0) {
      if (errno == EINTR) continue;
      close_clients();
      fail(
          errno_string("poll failed while finalizing LCT torchrun TCP "
                       "rendezvous"));
    }

    for (size_t i = 0; i < pfds.size(); ++i) {
      if (pfds[i].revents == 0) continue;
      int client_index = client_indices[i];
      client_conn_t& client = g_clients[client_index];
      if (pfds[i].revents & POLLNVAL) {
        done[client_index] = true;
        ++done_count;
        continue;
      }
      if (pfds[i].revents & POLLERR) {
        close_clients();
        fail("LCT torchrun PMI finalize saw socket error for rank " +
             std::to_string(client.rank));
      }
      if (!(pfds[i].revents & (POLLIN | POLLHUP))) continue;

      wire_msg_t msg;
      std::string error;
      if (!recv_msg(client.fd, &msg, deadline, &error)) {
        // A peer that has already closed has no more rendezvous state to clean
        // up. Treat EOF during finalize as completion so rank 0 does not leave
        // resources open waiting for a process that is already gone.
        done[client_index] = true;
        ++done_count;
        continue;
      }
      if (msg.type != MSG_FINALIZE) {
        close_clients();
        fail("LCT torchrun PMI received unexpected request type " +
             std::to_string(static_cast<uint32_t>(msg.type)) +
             " while finalizing rank " + std::to_string(client.rank));
      }
      done[client_index] = true;
      ++done_count;
    }
  }
  close_clients();
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

void reset_runtime_state()
{
  close_fd(&g_client_fd);
  close_clients();
  g_pending_kvs.clear();
  g_kvs.clear();
  g_rank = -1;
  g_size = -1;
  g_local_rank = -1;
  g_local_size = -1;
  g_master_addr.clear();
  g_master_port = -1;
  g_endpoint_source.clear();
  g_timeout_sec = k_default_timeout_sec;
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
  reset_runtime_state();
  parse_environment();
  deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);

  if (g_rank == 0) {
    int listen_fd = -1;
    try {
      listen_fd = create_listener(g_master_port);
      accept_rank_clients(listen_fd, deadline);
      close_fd(&listen_fd);
    } catch (const std::exception& e) {
      send_error_to_clients(e.what(), deadline);
      close_fd(&listen_fd);
      close_clients();
      fail("LCT torchrun PMI failed during TCP rendezvous setup: " +
           std::string(e.what()));
    }
  } else {
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
  }

  LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi_torchrun",
          "Connected rank %d/%d to TCP rendezvous %s:%d (local rank %d/%d, "
          "timeout %ds, zero background threads).\n",
          g_rank, g_size, g_master_addr.c_str(), g_master_port, g_local_rank,
          g_local_size, g_timeout_sec);
}

int initialized() { return g_rank >= 0; }
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

  // PMI-style put: no socket traffic here. The pending local entry becomes
  // globally visible only when the next barrier/fence synchronously exchanges
  // pending entries and returns a post-fence KVS snapshot.
  g_pending_kvs[key] = value;
}

void getname(int rank, char* key, char* value)
{
  fail_if(!initialized(), "LCT torchrun PMI getname called before initialize");
  fail_if(key == nullptr || value == nullptr,
          "LCT torchrun PMI getname received a null key/value");
  fail_if(rank < 0 || rank >= g_size,
          "LCT torchrun PMI getname requested invalid rank " +
              std::to_string(rank) + " for key '" + key + "'");
  fail_if(strlen(key) >= LCT_PMI_STRING_LIMIT,
          "LCT torchrun PMI getname key exceeds LCT_PMI_STRING_LIMIT");

  // PMI-style get: this is deliberately a local cache lookup. Applications
  // must call barrier() after publish() before asking for remote keys.
  auto it = g_kvs.find(std::make_pair(rank, std::string(key)));
  fail_if(it == g_kvs.end(),
          "LCT torchrun PMI key '" + std::string(key) + "' from rank " +
              std::to_string(rank) +
              " was not found in the post-barrier KVS snapshot. Ensure all "
              "ranks call LCT_pmi_publish before LCT_pmi_barrier.");
  fail_if(it->second.size() >= LCT_PMI_STRING_LIMIT,
          "LCT torchrun PMI getname value exceeds LCT_PMI_STRING_LIMIT");
  strcpy(value, it->second.c_str());
}

void barrier()
{
  fail_if(!initialized(), "LCT torchrun PMI barrier called before initialize");

  // PMI-style fence: this is the only regular operation that performs TCP
  // progress. Rank 0 runs the rendezvous server synchronously in this call via
  // poll(); nonzero ranks send their pending puts, wait for rank 0's fence, and
  // receive a local KVS snapshot for subsequent getname() calls.
  if (g_rank == 0)
    rank0_barrier();
  else
    nonzero_barrier();
}

void finalize()
{
  if (!initialized()) return;

  if (g_rank == 0) {
    rank0_finalize();
  } else if (g_client_fd >= 0) {
    deadline_t deadline = clock_t::now() + std::chrono::seconds(g_timeout_sec);
    wire_msg_t msg;
    msg.type = MSG_FINALIZE;
    msg.rank = g_rank;
    std::string error;
    send_msg(g_client_fd, msg, deadline, &error);
    close_fd(&g_client_fd);
  }
  reset_runtime_state();
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
