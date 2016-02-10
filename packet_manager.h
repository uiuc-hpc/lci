#ifndef PACKET_MANAGER_H_
#define PACKET_MANAGER_H_

enum mpiv_packet_type { SEND_SHORT, SEND_READY, RECV_READY, SEND_READY_FIN };

struct mpiv_packet_header {
  mpiv_packet_type type;
  int from;
  int tag;
} __attribute__((aligned(8)));

struct mpiv_packet_content {
  union {
    char buffer[SHORT];
    // For rdz.
    struct {
      uintptr_t sreq;
      uintptr_t rreq;
      uintptr_t tgt_addr;
      uint32_t rkey;
    } rdz;
  };
};

class mpiv_packet {
 public:
  mpiv_packet() {}
  mpiv_packet_header header;

  inline void set_bytes(void* bytes, const size_t& size) {
    memcpy(content.buffer, bytes, size);
  }

  inline char* buffer() { return content.buffer; }

  inline void set_sreq(const uintptr_t& sreq) { content.rdz.sreq = sreq; }

  inline uintptr_t rdz_tgt_addr() { return content.rdz.tgt_addr; }

  inline uintptr_t rdz_sreq() { return content.rdz.sreq; }

  inline uintptr_t rdz_rreq() { return content.rdz.rreq; }

  inline uintptr_t rdz_rkey() { return content.rdz.rkey; }

  inline void set_rdz(uintptr_t sreq, uintptr_t rreq, uintptr_t tgt_addr,
    uint32_t rkey) {
    content.rdz = {sreq, rreq, tgt_addr, rkey};
  }

 private:
  mpiv_packet_content content;
} __attribute__((aligned(64)));

class packet_manager final {
 public:
  inline mpiv_packet* get_packet() {
    mpiv_packet* packet;
    if (!pool_.pop(packet)) {
      throw packet_error(
          "Not enough buffer, consider increasing concurrency level");
    }
    return packet;
  }

  inline mpiv_packet* get_packet(mpiv_packet_type packet_type, int rank,
                                 int tag) {
    mpiv_packet* packet = get_packet();
    packet->header = {packet_type, rank, tag};
    return packet;
  }

  inline mpiv_packet* get_packet(char buf[], mpiv_packet_type packet_type,
                                 int rank, int tag) {
    mpiv_packet* packet = reinterpret_cast<mpiv_packet*>(buf);
    packet->header = {packet_type, rank, tag};
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
