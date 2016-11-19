#include "mv.h"
#include "mv-inl.h"

void* mv_malloc(mv_engine* mv, size_t size) {
  return mv_server_alloc(mv->server, size);
}

void mv_free(mv_engine* mv, void* ptr) {
  mv_server_dealloc(mv->server, ptr);
}

void mv_set_num_worker(mv_engine* mv, int number) {
  mv_pp_ext(mv->pkpool, number);
}
