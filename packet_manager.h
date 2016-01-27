#ifndef PACKET_MANAGER_H_
#define PACKET_MANAGER_H_

enum mpiv_packet_type {
    SEND_SHORT, SEND_READY, RECV_READY,
    SEND_READY_FIN
};

struct mpiv_packet_header {
    mpiv_packet_type type;
    int from;
    int tag;
} __attribute__((aligned(8)));

struct mpiv_packet {
    mpiv_packet_header header;
    union {
        struct {
            char buffer[SHORT];
        } egr;
        struct {
            uintptr_t sreq;
            uintptr_t rreq;
            uintptr_t tgt_addr;
            uint32_t rkey;
            uint32_t size;
        } rdz;
    };
} __attribute__ ((aligned(64)));


class packet_manager final {
 public:
  inline mpiv_packet* get_packet() {
    mpiv_packet* packet;
    if (!squeue_.pop(packet)) {
        throw new std::runtime_error("Not enough buffer, consider increasing concurrency level");
    }
    return packet;
  }

  inline mpiv_packet* get_packet(mpiv_packet_type packet_type, int rank, int tag) {
    mpiv_packet* packet = get_packet();
    packet->header = {packet_type, rank, tag};
    return packet;
  }

  inline mpiv_packet* get_packet(char buf[], mpiv_packet_type packet_type, int rank, int tag) {
    mpiv_packet* packet = reinterpret_cast<mpiv_packet*>(buf);
    packet->header = {packet_type, rank, tag};
    return packet;
  }

  inline void new_packet(mpiv_packet* packet) {
    if (!squeue_.push(packet)) {
        throw new std::runtime_error("Fatal error, insert more than possible packets into manager");
    }
  }

 private:
  boost::lockfree::stack<mpiv_packet*, boost::lockfree::capacity<NSBUF>> squeue_;
};

#endif
