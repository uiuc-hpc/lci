#ifndef PACKET_MANAGER_H_
#define PACKET_MANAGER_H_

#include "packet.h"
#include "mpmcqueue.h"

extern int mpiv_worker_id();

class packet_manager_base {
 public:
  virtual void init_worker(int) = 0;
  virtual mpiv_packet* get_packet_nb() = 0;
  virtual mpiv_packet* get_packet() = 0;
  virtual void ret_packet(mpiv_packet* packet) = 0;
  virtual mpiv_packet* get_for_send() = 0;
  virtual mpiv_packet* get_for_recv() = 0;
  virtual void ret_packet_to(mpiv_packet* packet, int hint) = 0;
} __attribute__((aligned(64)));

class packet_manager_MPMCQ : public packet_manager_base {
 public:
  void init_worker(int) override {}

  inline mpiv_packet* get_packet_nb() override {
    if (pool_.empty()) return 0;
    return (mpiv_packet*)pool_.dequeue();
  }

  inline mpiv_packet* get_packet() override {
    mpiv_packet* p = 0;
    while (!(p = get_packet_nb())) ult_yield();
    return p;
  }

  inline void ret_packet(mpiv_packet* packet) override {
    assert(packet != 0);
    pool_.enqueue((uint64_t)packet);
  }

  inline mpiv_packet* get_for_send() override { return get_packet(); }
  inline mpiv_packet* get_for_recv() override { return get_packet_nb(); }
  inline void ret_packet_to(mpiv_packet* packet, int) override {
    ret_packet(packet);
  }

 protected:
  ppl::MPMCQueue<uint64_t> pool_;
} __attribute__((aligned(64)));

class packet_manager_LFQUEUE: public packet_manager_base {
 public:

  void init_worker(int) {};

  inline mpiv_packet* get_packet_nb() override {
    mpiv_packet* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline mpiv_packet* get_packet() override {
    mpiv_packet* packet = NULL;
    while (!pool_.pop(packet)) ult_yield();
    assert(packet);
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
  boost::lockfree::queue<mpiv_packet*,
                         boost::lockfree::capacity<MAX_CONCURRENCY>> pool_;
} __attribute__((aligned(64)));

class packet_manager_LFSTACK : public packet_manager_base {
 public:

  void init_worker(int) {};

  inline mpiv_packet* get_packet_nb() override {
    mpiv_packet* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline mpiv_packet* get_packet() override {
    mpiv_packet* packet = NULL;
    while (!pool_.pop(packet)) ult_yield();
    assert(packet);
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
} __attribute__((aligned(64)));

template <class T>
class arr_pool {
 public:
  static const size_t MAX_SIZE = (1 << 12);
  arr_pool(size_t max_size)
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
  size_t max_size_;
  size_t top_;
  size_t bottom_;
  T* container_;
} __attribute__((aligned(64)));

class packet_manager_NUMA final
    : public packet_manager_LFSTACK {
 public:

  using parent = packet_manager_LFSTACK;

  packet_manager_NUMA() : nworker_(0){};

  ~packet_manager_NUMA() {
    for (auto& a : private_pool_) delete a;
  }

  void init_worker(int nworker) {
    for (int i = nworker_; i < nworker; i++) {
      private_pool_.emplace_back(
          new arr_pool<mpiv_packet*>(MAX_SEND / nworker / 2));
    }
    nworker_ = nworker;
  }

  using parent::get_packet_nb;
  using parent::get_packet;
  using parent::ret_packet;

  inline mpiv_packet* get_for_send() override {
    mpiv_packet* p = 0;
    p = private_pool_[mpiv_worker_id()]->popTop();
    if (!p) p = get_packet();
    p->poolid() = mpiv_worker_id();
    return p;
  }

  inline mpiv_packet* get_for_recv() override {
    mpiv_packet* p = 0;
    if (!(p = get_packet_nb())) {
      int steal = rand() % nworker_;
      p = private_pool_[steal]->popBottom();
    }
    return p;
  }

  inline void ret_packet_to(mpiv_packet* packet, int hint) override {
    mpiv_packet* p = private_pool_[hint]->pushTop(packet);
    if (p) {
      ret_packet(p);
    }
  }

 private:
  std::vector<arr_pool<mpiv_packet*>*> private_pool_;
  int nworker_;
} __attribute__((aligned(64)));

class packet_manager_NUMA_STEAL final
    : public packet_manager_base {
 public:

  packet_manager_NUMA_STEAL() {
    private_pool_.emplace_back(
        new arr_pool<mpiv_packet*>(MAX_CONCURRENCY));
    nworker_ = 1;
  };

  ~packet_manager_NUMA_STEAL() {
    for (auto& a : private_pool_) delete a;
  }

  void init_worker(int nworker) {
    for (int i = nworker_; i < nworker + 1; i++) {
      private_pool_.emplace_back(
          new arr_pool<mpiv_packet*>(MAX_CONCURRENCY));
    }
    nworker_ = nworker;
  }

  inline void ret_packet(mpiv_packet* p) override {
    private_pool_[0]->pushTop(p);
  }

  inline mpiv_packet* get_packet_nb() override {
    mpiv_packet* p = private_pool_[0]->popBottom();
    if (!p) {
      int steal = rand() % (nworker_ + 1);
      return private_pool_[steal]->popBottom();
    }
    return p;
  }

  inline mpiv_packet* get_packet() override {
    mpiv_packet* p = 0;
    while (!(p = get_packet_nb())) { ult_yield(); }
    return p;
  }

  inline mpiv_packet* get_for_send() override {
    mpiv_packet* p = private_pool_[mpiv_worker_id() + 1]->popTop();
    if (!p) p = get_packet();
    p->poolid() = mpiv_worker_id();
    return p;
  }

  inline mpiv_packet* get_for_recv() override {
    mpiv_packet* p = private_pool_[0]->popTop();
    if (!p) p = get_packet_nb();
    return p;
  }

  inline void ret_packet_to(mpiv_packet* packet, int hint) override {
    private_pool_[hint + 1]->pushTop(packet);
  }

 private:
  std::vector<arr_pool<mpiv_packet*>*> private_pool_;
  int nworker_;
} __attribute__((aligned(64)));

#ifdef CONFIG_PK_MANAGER 
using packet_manager = CONFIG_PK_MANAGER;
#else
using packet_manager = packet_manager_NUMA_STEAL;
#endif

#endif
