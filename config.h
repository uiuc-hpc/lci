#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdlib.h>

namespace mpiv {

static const int MAX_SEND = 64;  // maximum concurrent send.
static const int MAX_RECV = 32;  // maximum concurrent recv.

#ifndef CONFIG_CONCUR
static const int MAX_CONCURRENCY = MAX_SEND + MAX_RECV;
#else
static const int MAX_CONCURRENCY = CONFIG_CONCUR;
#endif

static const int PACKET_SIZE = (16 * 1024 + 64);       // transfer unit size.
static const int SHORT_MSG_SIZE = (PACKET_SIZE - 16);  // short message size.
static const int RNDZ_MSG_SIZE = 48;

static const size_t HEAP_SIZE =
    (size_t)2 * 1024 * 1024 * 1024;  // total pinned heap size.

/*! This config is to select particular implementation for each component. */
enum class ConfigType {
  // Server.
  SERVER_RDMAX = 0,
  SERVER_OFI,
  // HashTable.
  HASHTBL_ARR,
  HASHTBL_COCK,
  // Packet Manager.
  PACKET_MANAGER_NUMA_STEAL,
};

template <ConfigType>
struct Config;

constexpr ConfigType ServerCfg = ConfigType::SERVER_RDMAX;
constexpr ConfigType HashTblCfg = ConfigType::HASHTBL_ARR;
constexpr ConfigType PacketManagerCfg = ConfigType::PACKET_MANAGER_NUMA_STEAL;

}  // namespace mpiv.

#endif
