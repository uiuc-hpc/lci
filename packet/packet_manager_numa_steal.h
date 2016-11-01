#ifndef PACKET_MANAGER_NUMA_STEAL_H_
#define PACKET_MANAGER_NUMA_STEAL_H_

template <class T>
class ArrPool {
 public:
  static const size_t MAX_SIZE = (1 << 12);
  ArrPool(size_t max_size)
      : lock_flag(ATOMIC_FLAG_INIT),
        max_size_(max_size),
        top_(0),
        bottom_(0),
        container_(new T[MAX_SIZE]) {
    memset(container_, 0, MAX_SIZE * sizeof(T));
  }

  ~ArrPool() { delete[] container_; }

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

class PacketManagerNumaSteal final : public PacketManagerBase {
 public:
  PacketManagerNumaSteal() {
    private_pool_.emplace_back(new ArrPool<Packet*>(MAX_CONCURRENCY));
    nworker_ = 1;
  };

  ~PacketManagerNumaSteal() {
    for (auto& a : private_pool_) delete a;
  }

  void init_worker(int nworker) {
    for (int i = nworker_; i < nworker + 1; i++) {
      private_pool_.emplace_back(new ArrPool<Packet*>(MAX_CONCURRENCY));
    }
    nworker_ = nworker;
  }

  inline void ret_packet(Packet* p) override { private_pool_[0]->pushTop(p); }

  inline Packet* get_packet_nb() override {
    Packet* p = private_pool_[0]->popBottom();
    if (!p) {
      int steal = rand() % (nworker_ + 1);
      return private_pool_[steal]->popBottom();
    }
    return p;
  }

  inline Packet* get_packet() override {
    Packet* p = 0;
    while (!(p = get_packet_nb())) {
      ult_yield();
    }
    return p;
  }

  inline Packet* get_for_send() override {
    Packet* p = private_pool_[worker_id() + 1]->popTop();
    if (!p) p = get_packet();
    p->poolid() = worker_id();
    return p;
  }

  inline Packet* get_for_recv() override {
    Packet* p = private_pool_[0]->popTop();
    if (!p) p = get_packet_nb();
    return p;
  }

  inline void ret_packet_to(Packet* packet, int hint) override {
    private_pool_[hint + 1]->pushTop(packet);
  }

 private:
  std::vector<ArrPool<Packet*>*> private_pool_;
  int nworker_;
} __attribute__((aligned(64)));

#endif
