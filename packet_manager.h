#ifndef PACKET_MANAGER_H_
#define PACKET_MANAGER_H_

#include "common.h"

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
    header_ = {type, from, tag};
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

class packet_manager final {
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

 private:
  boost::lockfree::stack<mpiv_packet*, boost::lockfree::capacity<NSBUF>> pool_;
};

#endif
