#ifndef PACKET_MANAGER_NUMA_MISC_H_
#define PACKET_MANAGER_NUMA_MISC_H_

#include "mpmcqueue.h"

class PacketManagerMPMCQ : public PacketManagerBase {
 public:
  void init_worker(int) override {}

  inline Packet* get_packet_nb() override {
    if (pool_.empty()) return 0;
    return (Packet*)pool_.dequeue();
  }

  inline Packet* get_packet() override {
    Packet* p = 0;
    while (!(p = get_packet_nb())) ult_yield();
    return p;
  }

  inline void ret_packet(Packet* packet) override {
    assert(packet != 0);
    pool_.enqueue((uint64_t)packet);
  }

  inline Packet* get_for_send() override { return get_packet(); }
  inline Packet* get_for_recv() override { return get_packet_nb(); }
  inline void ret_packet_to(Packet* packet, int) override {
    ret_packet(packet);
  }

 protected:
  ppl::MPMCQueue<uint64_t> pool_;
} __attribute__((aligned(64)));

class PacketManagerLfQueue: public PacketManagerBase {
 public:

  void init_worker(int) {};

  inline Packet* get_packet_nb() override {
    Packet* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline Packet* get_packet() override {
    Packet* packet = NULL;
    while (!pool_.pop(packet)) ult_yield();
    assert(packet);
    return packet;
  }

  inline void ret_packet(Packet* packet) override {
    if (!pool_.push(packet)) {
      throw packet_error(
          "Fatal error, insert more than possible packets into manager");
    }
  }

  inline Packet* get_for_send() override { return get_packet(); }
  inline Packet* get_for_recv() override { return get_packet_nb(); }
  inline void ret_packet_to(Packet* packet, int) override {
    ret_packet(packet);
  }

 protected:
  boost::lockfree::queue<Packet*,
                         boost::lockfree::capacity<MAX_CONCURRENCY>> pool_;
} __attribute__((aligned(64)));

class PacketManagerLfStack : public PacketManagerBase {
 public:

  void init_worker(int) {};

  inline Packet* get_packet_nb() override {
    Packet* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline Packet* get_packet() override {
    Packet* packet = NULL;
    while (!pool_.pop(packet)) ult_yield();
    assert(packet);
    return packet;
  }

  inline void ret_packet(Packet* packet) override {
    if (!pool_.push(packet)) {
      throw packet_error(
          "Fatal error, insert more than possible packets into manager");
    }
  }

  inline Packet* get_for_send() override { return get_packet(); }
  inline Packet* get_for_recv() override { return get_packet_nb(); }
  inline void ret_packet_to(Packet* packet, int) override {
    ret_packet(packet);
  }

 protected:
  boost::lockfree::stack<Packet*,
                         boost::lockfree::capacity<MAX_CONCURRENCY>> pool_;
} __attribute__((aligned(64)));

class PacketManagerNuma final
    : public PacketManagerLfStack {
 public:

  using parent = PacketManagerLfStack;

  PacketManagerNuma() : nworker_(0){};

  ~PacketManagerNuma() {
    for (auto& a : private_pool_) delete a;
  }

  void init_worker(int nworker) {
    for (int i = nworker_; i < nworker; i++) {
      private_pool_.emplace_back(
          new ArrPool<Packet*>(MAX_SEND / nworker / 2));
    }
    nworker_ = nworker;
  }

  using parent::get_packet_nb;
  using parent::get_packet;
  using parent::ret_packet;

  inline Packet* get_for_send() override {
    Packet* p = 0;
    p = private_pool_[worker_id()]->popTop();
    if (!p) p = get_packet();
    p->poolid() = worker_id();
    return p;
  }

  inline Packet* get_for_recv() override {
    Packet* p = 0;
    if (!(p = get_packet_nb())) {
      int steal = rand() % nworker_;
      p = private_pool_[steal]->popBottom();
    }
    return p;
  }

  inline void ret_packet_to(Packet* packet, int hint) override {
    Packet* p = private_pool_[hint]->pushTop(packet);
    if (p) {
      ret_packet(p);
    }
  }

 private:
  std::vector<ArrPool<Packet*>*> private_pool_;
  int nworker_;
} __attribute__((aligned(64)));


#endif
