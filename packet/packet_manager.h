#ifndef PACKET_MANAGER_H_
#define PACKET_MANAGER_H_

#include "packet.h"

namespace mpiv {

class PacketManagerBase {
 public:
  virtual void init_worker(int) = 0;
  virtual Packet* get_packet_nb() = 0;
  virtual Packet* get_packet() = 0;
  virtual void ret_packet(Packet* packet) = 0;
  virtual Packet* get_for_send() = 0;
  virtual Packet* get_for_recv() = 0;
  virtual void ret_packet_to(Packet* packet, int hint) = 0;
} __attribute__((aligned(64)));

#include "packet_manager_numa_steal.h"

template <>
struct Config<ConfigType::PACKET_MANAGER_NUMA_STEAL> {
  using PacketManager = PacketManagerNumaSteal;
};

using PacketManager = Config<PacketManagerCfg>::PacketManager;

};  // namespace mpiv.

#endif
