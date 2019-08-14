#ifndef PACKET_MANAGER_NUMA_MISC_H_
#define PACKET_MANAGER_NUMA_MISC_H_

#include "mpmcqueue.h"
#include <assert.h>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>

class packetManagerMPMCQ
{
 public:
  void thread_yield() {}
  void init_worker(int) {}
  inline void* get_packet_nb()
  {
    if (pool_.empty()) return 0;
    return (void*)pool_.dequeue();
  }

  inline void* get_packet()
  {
    void* p = 0;
    while (!(p = get_packet_nb())) thread_yield();
    return p;
  }

  inline void ret_packet(void* packet)
  {
    assert(packet != 0);
    pool_.enqueue((uint64_t)packet);
  }

  inline void* get_for_send() { return get_packet(); }
  inline void* get_for_recv() { return get_packet_nb(); }
  inline void ret_packet_to(void* packet, int) { ret_packet(packet); }

 protected:
  ppl::MPMCQueue<uint64_t> pool_;
} __attribute__((aligned(64)));

class packetManagerLfQueue
{
 public:
  void init_worker(int){};
  void thread_yield() {}
  inline void* get_packet_nb()
  {
    void* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline void* get_packet()
  {
    void* packet = NULL;
    while (!pool_.pop(packet)) thread_yield();
    assert(packet);
    return packet;
  }

  inline void ret_packet(void* packet)
  {
    if (!pool_.push(packet)) {
      assert(0);
    }
  }

  inline void* get_for_send() { return get_packet(); }
  inline void* get_for_recv() { return get_packet_nb(); }
  inline void ret_packet_to(void* packet, int) { ret_packet(packet); }

 protected:
  boost::lockfree::queue<void*, boost::lockfree::capacity<MAX_PACKET>> pool_;
} __attribute__((aligned(64)));

class packetManagerLfStack
{
 public:
  void init_worker(int){};

  inline void* get_packet_nb()
  {
    void* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline void* get_packet()
  {
    void* packet = NULL;
    while (!pool_.pop(packet)) thread_yield();
    assert(packet);
    return packet;
  }

  inline void ret_packet(void* packet)
  {
    if (!pool_.push(packet)) {
      assert(0);
    }
  }

  inline void* get_for_send() { return get_packet(); }
  inline void* get_for_recv() { return get_packet_nb(); }
  inline void ret_packet_to(void* packet, int) { ret_packet(packet); }

 protected:
  boost::lockfree::stack<void*, boost::lockfree::capacity<MAX_PACKET>> pool_;
} __attribute__((aligned(64)));

#endif
