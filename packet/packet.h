#ifndef PACKET_H_
#define PACKET_H_

enum mpiv_packet_type { SEND_SHORT, SEND_READY, RECV_READY, SEND_READY_FIN };

struct mpiv_packet_header {
  mpiv_packet_type type;
  uint8_t poolid;
  int from;
  int tag;
} __attribute__((aligned(8)));

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

class mpiv_packet {
 public:
  mpiv_packet() {}

  inline const mpiv_packet_header& header() { return header_; }

  inline void set_header(mpiv_packet_type type, int from, int tag) {
    header_.type = type;
    header_.from = from;
    header_.tag = tag;
  }

  inline uint8_t& poolid() { return header_.poolid; }

  inline void set_bytes(const void* bytes, const int& size) {
    memcpy(content_.buffer, bytes, size);
  }

  inline char* buffer() { return content_.buffer; }

  inline void set_sreq(const uintptr_t& sreq) { content_.rdz.sreq = sreq; }

  inline uintptr_t rdz_tgt_addr() { return content_.rdz.tgt_addr; }

  inline uintptr_t rdz_sreq() { return content_.rdz.sreq; }

  inline uintptr_t rdz_rreq() { return content_.rdz.rreq; }

  inline uintptr_t rdz_rkey() { return content_.rdz.rkey; }

  inline mpiv_key get_key() { return mpiv_make_key(header_.from, header_.tag); }

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
} __attribute__((aligned(64)));

#endif
