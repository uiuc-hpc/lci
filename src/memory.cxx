#include "mv.h"
#include "mv-inl.h"

void* mv_heap_ptr(mv_engine* mv) {
  return mv_server_heap_ptr(mv->server);
}

void mv_set_num_worker(mv_engine* mv, int number) {
  mv_pp_ext(mv->pkpool, number);
}
