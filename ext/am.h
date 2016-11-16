#ifndef EXT_AM_H_
#define EXT_AM_H_

namespace mpiv {
void am_send(size_t node, uint8_t fid, void* src, size_t size, int8_t count, ...) {
  va_list args;
  va_start(args, count);

  Packet* packet = MPIV.pkpool.get_for_send();
  packet->set_header(SEND_AM, MPIV.me, fid);
  uint32_t* buffer = (uint32_t*) packet->buffer();
  buffer[0] = size;
  buffer[1] = (uint32_t) count;
  for (int i = 0; i < count; i++) {
    buffer[i + 2] = va_arg(args, uint32_t);
  }
  memcpy((void*) &buffer[count + 2], src, size);

  MPIV.server.write_send(node, (void*) packet, (count + 2) * sizeof(uint32_t) + (uint32_t)size + sizeof(PacketHeader), 0);
  va_end(args);
}

void put(size_t node, void* dst, void* src, size_t size) {
  MPIV.server.write_rma(node, src, MPIV.server.heap_lkey(), dst, MPIV.server.heap_rkey(node), size, 0);
}

void put_signal(size_t node, void* dst, void* src, size_t size, uint32_t sid) {
  MPIV.server.write_rma_signal(node, src, MPIV.server.heap_lkey(), dst, MPIV.server.heap_rkey(node), size, sid, 0);
}

uint8_t am_register(am_func_t f) {
  MPI_Barrier(MPI_COMM_WORLD);
  MPIV.am_table.push_back(f);
  MPI_Barrier(MPI_COMM_WORLD);
  return MPIV.am_table.size() - 1;
}

}

#endif