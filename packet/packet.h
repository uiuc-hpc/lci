#ifndef PACKET_H_
#define PACKET_H_

enum packetType { SEND_SHORT, SEND_READY, RECV_READY, SEND_READY_FIN, SEND_AM };

struct packet_header {
  packetType type;
  uint8_t poolid;
  int from;
  int tag;
} __attribute__((aligned(8)));

struct mv_rdz {
  uintptr_t sreq;
  uintptr_t rreq;
  uintptr_t tgt_addr;
  uint32_t rkey;
};

union packet_content {
  char buffer[SHORT_MSG_SIZE];
  mv_rdz rdz;
};

class packet {
 public:
  packet() {}

  inline packet_header& header() { return header_; }

  inline void set_header(packetType type, int from, int tag) {
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

  inline mv_key get_key() { return mv_make_key(header_.from, header_.tag); }

  inline mv_key get_rdz_key() {
    return mv_make_key(header_.from, (1 << 31) | header_.tag);
  }

  inline void set_rdz(uintptr_t sreq, uintptr_t rreq, uintptr_t tgt_addr,
                      uint32_t rkey) {
    content_.rdz = {sreq, rreq, tgt_addr, rkey};
  }

 private:
  packet_header header_;
  packet_content content_;
} __attribute__((aligned(64)));

#endif
