#ifndef PACKET_MANAGER_H_
#define PACKET_MANAGER_H_

#include "common.h"
#include "mpmcqueue.h"

enum mpiv_packet_type { SEND_SHORT, SEND_READY, RECV_READY, SEND_READY_FIN };

struct alignas(8) mpiv_packet_header {
  mpiv_packet_type type;
  int from;
  int tag;
};

struct mpiv_rdz {
  uintptr_t sreq;
  uintptr_t rreq;
  uintptr_t tgt_addr;
  uint32_t rkey;
};

union mpiv_packet_content {
  char buffer[SHORT_MSG_SIZE];
  mpiv_rdz rdz;
};

class alignas(64) mpiv_packet {
 public:
  mpiv_packet() {}

  inline const mpiv_packet_header& header() {
    return header_;
  }

  inline void set_header(mpiv_packet_type type, int from, int tag) {
    header_.type = type;
    header_.from = from;
    header_.tag = tag;
  }

  inline void set_bytes(const void* bytes, const int& size) {
    memcpy(content_.buffer, bytes, size);
  }

  inline char* buffer() { return content_.buffer; }

  inline void set_sreq(const uintptr_t& sreq) { content_.rdz.sreq = sreq; }

  inline uintptr_t rdz_tgt_addr() { return content_.rdz.tgt_addr; }

  inline uintptr_t rdz_sreq() { return content_.rdz.sreq; }

  inline uintptr_t rdz_rreq() { return content_.rdz.rreq; }

  inline uintptr_t rdz_rkey() { return content_.rdz.rkey; }

  inline mpiv_key get_key() {
    return mpiv_make_key(header_.from, header_.tag);
  }

  inline mpiv_key get_rdz_key() {
    return mpiv_make_key(header_.from, (1 << 31) | header_.tag);
  }

  inline void set_rdz(uintptr_t sreq, uintptr_t rreq, uintptr_t tgt_addr,
    uint32_t rkey) {
    content_.rdz = {sreq, rreq, tgt_addr, rkey};
  }

 private:
  mpiv_packet_header header_;
  mpiv_packet_content content_;
};

class alignas(64) packet_managerZ final {
 public:
  packet_managerZ() {}

  inline mpiv_packet* get_packet_nb() {
    if (queue_.empty()) return 0;
    return (mpiv_packet*) queue_.dequeue();
  }

  inline mpiv_packet* get_packet() {
    mpiv_packet* p = 0;
    while (!(p = get_packet_nb())) fult_yield();
    return p;
  }

  inline mpiv_packet* get_packet(mpiv_packet_type packet_type, int rank,
                                 int tag) {
    mpiv_packet* packet = get_packet();
    packet->set_header(packet_type, rank, tag);
    return packet;
  }

  inline void new_packet(mpiv_packet* packet) {
    assert(packet != 0);
    queue_.enqueue((uint64_t) packet);
  }

  inline void ret_packet(mpiv_packet* packet) { new_packet(packet); }

 private:
  ppl::MPMCQueue<uint64_t> queue_;
};


inline uint8_t upper_power_of_two(int v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

class packet_managerY final {
 public:
  using pool_type = boost::lockfree::stack<mpiv_packet*, boost::lockfree::capacity<MAX_SEND+MAX_RECV>>;

  packet_managerY() { size_ = 1; pool_ = std::move(std::vector<pool_type>(1)); }

  inline void resize(int nworker) {
    size_ = upper_power_of_two(nworker);
    if (size_ > 1) { 
      std::vector<pool_type> t(size_);
      for (auto &pl : pool_) {
        mpiv_packet* p;
        while (pl.pop(p)) {
          t[lb_++ & (size_ -1)].push(p);
        }
      }
      pool_ = std::move(t);
    }
  }

  inline mpiv_packet* get_packet_nb() {
    mpiv_packet* packet = NULL;
    uint8_t lb = (lb_.fetch_sub(1)) & (size_ - 1);
    pool_[lb].pop(packet);
    return packet;
  }

  inline mpiv_packet* get_packet() {
    mpiv_packet* packet = NULL;
    while (!(packet = get_packet_nb())) fult_yield();
    return packet;
  }

  inline mpiv_packet* get_packet(mpiv_packet_type packet_type, int rank,
                                 int tag) {
    mpiv_packet* packet = get_packet();
    packet->set_header(packet_type, rank, tag);
    return packet;
  }

  inline void new_packet(mpiv_packet* packet) {
    uint8_t lb = (lb_.fetch_add(1) + 1) & (size_ - 1);
    if (!pool_[lb].push(packet)) {
      throw packet_error(
          "Fatal error, insert more than possible packets into manager");
    }
  }

  inline void ret_packet(mpiv_packet* packet) { new_packet(packet); }

 private:
  vector<pool_type> pool_;
  std::atomic<uint8_t> lb_;
  uint8_t size_;
};

class packet_managerX final {
 public:

  inline mpiv_packet* get_packet_nb() {
    mpiv_packet* packet = NULL;
    pool_.pop(packet);
    return packet;
  }

  inline mpiv_packet* get_packet() {
    mpiv_packet* packet = NULL;
    while (!pool_.pop(packet)) fult_yield();
    return packet;
  }

  inline mpiv_packet* get_packet(mpiv_packet_type packet_type, int rank,
                                 int tag) {
    mpiv_packet* packet = get_packet();
    packet->set_header(packet_type, rank, tag);
    return packet;
  }

  inline void new_packet(mpiv_packet* packet) {
    if (!pool_.push(packet)) {
      throw packet_error(
          "Fatal error, insert more than possible packets into manager");
    }
  }

  inline void ret_packet(mpiv_packet* packet) { new_packet(packet); }

 private:
  boost::lockfree::stack<mpiv_packet*, boost::lockfree::capacity<MAX_SEND + MAX_RECV>> pool_;
};

using packet_manager = packet_managerX;
using packet_manager2 = packet_managerX;

#endif
