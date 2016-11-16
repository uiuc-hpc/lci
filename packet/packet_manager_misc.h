#ifndef PACKET_MANAGER_NUMA_MISC_H_
#define PACKET_MANAGER_NUMA_MISC_H_

#include "mpmcqueue.h"

class packetManagerMPMCQ : public packetManagerBase {
 public:
  void init_worker(int) override {}

  inline packet* get_packet_nb() override {
    if (pool_.empty()) return 0;
    return (packet*)pool_.dequeue();
  }

  inline packet* get_packet() override {
    packet* p = 0;
    while (!(p = get_packet_nb())) ult_yield();
    return p;
  }

  inline void ret_packet(packet* packet) override {
    assert(packet != 0);
    pool_.enqueue((uint64_t)packet);
  }

  inline packet* get_for_send() override { return get_packet(); }
  inline packet* get_for_recv() override { return get_packet_nb(); }
  inline void ret_packet_to(packet* packet, int) override {
    ret_packet(packet);
  }

 protected:
  ppl::MPMCQueue<uint64_t> pool_;
} __attribute__((aligned(64)));

class packetManagerLfQueue : public packetManagerBase {
 public:
  void init_worker(int){};

  inline packet* get_packet_nb() override {
    packet* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline packet* get_packet() override {
    packet* packet = NULL;
    while (!pool_.pop(packet)) ult_yield();
    assert(packet);
    return packet;
  }

  inline void ret_packet(packet* packet) override {
    if (!pool_.push(packet)) {
      throw packet_error(
          "Fatal error, insert more than possible packets into manager");
    }
  }

  inline packet* get_for_send() override { return get_packet(); }
  inline packet* get_for_recv() override { return get_packet_nb(); }
  inline void ret_packet_to(packet* packet, int) override {
    ret_packet(packet);
  }

 protected:
  boost::lockfree::queue<packet*, boost::lockfree::capacity<MAX_CONCURRENCY>>
      pool_;
} __attribute__((aligned(64)));

class packetManagerLfStack : public packetManagerBase {
 public:
  void init_worker(int){};

  inline packet* get_packet_nb() override {
    packet* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline packet* get_packet() override {
    packet* packet = NULL;
    while (!pool_.pop(packet)) ult_yield();
    assert(packet);
    return packet;
  }

  inline void ret_packet(packet* packet) override {
    if (!pool_.push(packet)) {
      throw packet_error(
          "Fatal error, insert more than possible packets into manager");
    }
  }

  inline packet* get_for_send() override { return get_packet(); }
  inline packet* get_for_recv() override { return get_packet_nb(); }
  inline void ret_packet_to(packet* packet, int) override {
    ret_packet(packet);
  }

 protected:
  boost::lockfree::stack<packet*, boost::lockfree::capacity<MAX_CONCURRENCY>>
      pool_;
} __attribute__((aligned(64)));

class packetManagerNuma final : public packetManagerLfStack {
 public:
  using parent = packetManagerLfStack;

  packetManagerNuma() : nworker_(0){};

  ~packetManagerNuma() {
    for (auto& a : private_pool_) delete a;
  }

  void init_worker(int nworker) {
    for (int i = nworker_; i < nworker; i++) {
      private_pool_.emplace_back(new ArrPool<packet*>(MAX_SEND / nworker / 2));
    }
    nworker_ = nworker;
  }

  using parent::get_packet_nb;
  using parent::get_packet;
  using parent::ret_packet;

  inline packet* get_for_send() override {
    packet* p = 0;
    p = private_pool_[worker_id()]->popTop();
    if (!p) p = get_packet();
    p->poolid() = worker_id();
    return p;
  }

  inline packet* get_for_recv() override {
    packet* p = 0;
    if (!(p = get_packet_nb())) {
      int steal = rand() % nworker_;
      p = private_pool_[steal]->popBottom();
    }
    return p;
  }

  inline void ret_packet_to(packet* packet, int hint) override {
    packet* p = private_pool_[hint]->pushTop(packet);
    if (p) {
      ret_packet(p);
    }
  }

 private:
  std::vector<ArrPool<packet*>*> private_pool_;
  int nworker_;
} __attribute__((aligned(64)));

#endif
