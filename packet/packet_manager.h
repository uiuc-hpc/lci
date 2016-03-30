#ifndef PACKET_MANAGER_H_
#define PACKET_MANAGER_H_

#include "packet.h"
#include "mpmcqueue.h"

class alignas(64) packet_manager_base {
 public:
  virtual mpiv_packet* get_packet_nb() = 0;
  virtual mpiv_packet* get_packet() = 0;
  virtual void ret_packet(mpiv_packet* packet) = 0;
  virtual mpiv_packet* get_for_send() = 0;
  virtual mpiv_packet* get_for_recv() = 0;
  virtual void ret_packet_to(mpiv_packet* packet, int hint) = 0;
};

class alignas(64) packet_manager_MPMCQ final : public packet_manager_base {
 public:
  inline mpiv_packet* get_packet_nb() override {
    if (queue_.empty()) return 0;
    return (mpiv_packet*)queue_.dequeue();
  }

  inline mpiv_packet* get_packet() override {
    mpiv_packet* p = 0;
    while (!(p = get_packet_nb())) fult_yield();
    return p;
  }

  inline void ret_packet(mpiv_packet* packet) override {
    assert(packet != 0);
    queue_.enqueue((uint64_t)packet);
  }

  inline mpiv_packet* get_for_send() override { return get_packet(); }
  inline mpiv_packet* get_for_recv() override { return get_packet_nb(); }
  inline void ret_packet_to(mpiv_packet* packet, int) override {
    ret_packet(packet);
  }

 private:
  ppl::MPMCQueue<uint64_t> queue_;
};

class alignas(64) packet_manager_LFSTACK : public packet_manager_base {
 public:
  inline mpiv_packet* get_packet_nb() override {
    mpiv_packet* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline mpiv_packet* get_packet() override {
    mpiv_packet* packet = NULL;
    while (!pool_.pop(packet)) fult_yield();
    return packet;
  }

  inline void ret_packet(mpiv_packet* packet) override {
    if (!pool_.push(packet)) {
      throw packet_error(
          "Fatal error, insert more than possible packets into manager");
    }
  }

  inline mpiv_packet* get_for_send() override { return get_packet(); }
  inline mpiv_packet* get_for_recv() override { return get_packet_nb(); }
  inline void ret_packet_to(mpiv_packet* packet, int) override {
    ret_packet(packet);
  }

 protected:
  boost::lockfree::stack<mpiv_packet*,
                         boost::lockfree::capacity<MAX_CONCURRENCY>> pool_;
};

template <class T>
class alignas(64) arr_pool {
 public:
  static const size_t MAX_SIZE = (1 << 12);
  arr_pool(uint8_t max_size)
      : lock_flag(ATOMIC_FLAG_INIT),
        max_size_(max_size),
        top_(0),
        bottom_(0),
        container_(new T[MAX_SIZE]) {
    memset(container_, 0, MAX_SIZE * sizeof(T));
  }

  ~arr_pool() { delete[] container_; }

  T popTop() {
    T ret = 0;
    lock();
    if (top_ != bottom_) {
      top_ = (top_ + MAX_SIZE - 1) & (MAX_SIZE - 1);
      ret = container_[top_];
    }
    unlock();
    return ret;
  };

  T pushTop(T p) {
    T ret = 0;
    lock();
    container_[top_] = p;
    top_ = (top_ + 1) & (MAX_SIZE - 1);
    if (((top_ + MAX_SIZE - bottom_) & (MAX_SIZE - 1)) > max_size_) {
      ret = container_[bottom_];
      bottom_ = (bottom_ + 1) & (MAX_SIZE - 1);
    }
    unlock();
    return ret;
  };

  T popBottom() {
    T ret = 0;
    lock();
    if (top_ != bottom_) {
      ret = container_[bottom_];
      bottom_ = (bottom_ + 1) & (MAX_SIZE - 1);
    }
    unlock();
    return ret;
  };

  inline void lock() {
    while (lock_flag.test_and_set(std::memory_order_acquire)) {
      asm volatile("pause\n" : : : "memory");
    }
  }

  inline void unlock() { lock_flag.clear(std::memory_order_release); }

 private:
  std::atomic_flag lock_flag;
  uint8_t max_size_;
  uint8_t top_;
  uint8_t bottom_;
  T* container_;
};

extern __thread int wid;

class alignas(64) packet_manager_NUMA_LFSTACK final
    : public packet_manager_LFSTACK {
 public:
  packet_manager_NUMA_LFSTACK() : nworker_(0){};
  ~packet_manager_NUMA_LFSTACK() {
    for (auto& a : private_pool_) {
      delete a;
    }
  }

  void init_worker(int nworker) {
    for (int i = nworker_; i < nworker; i++) {
      private_pool_.emplace_back(
          new arr_pool<mpiv_packet*>(MAX_SEND / nworker / 2));
    }
    nworker_ = nworker;
  }

  using packet_manager_LFSTACK::get_packet_nb;
  using packet_manager_LFSTACK::get_packet;
  using packet_manager_LFSTACK::ret_packet;

  inline mpiv_packet* get_for_send() override {
    mpiv_packet* p = 0;
    p = private_pool_[wid]->popTop();
    if (!p)
      return get_packet();
    else
      return p;
  }

  inline mpiv_packet* get_for_recv() override {
    mpiv_packet* p = 0;
    if (!pool_.pop(p)) {
      int steal = rand() % nworker_;
      p = private_pool_[steal]->popBottom();
    }
    return p;
  }

  inline void ret_packet_to(mpiv_packet* packet, int hint) override {
    mpiv_packet* p = private_pool_[hint]->pushTop(packet);
    if (p) {
      pool_.push(p);
    }
  }

 private:
  using packet_manager_LFSTACK::pool_;
  std::vector<arr_pool<mpiv_packet*>*> private_pool_;
  int nworker_;
};

using packet_manager = packet_manager_NUMA_LFSTACK;

#endif
