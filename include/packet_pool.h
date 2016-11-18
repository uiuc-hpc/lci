#ifndef PACKET_MANAGER_H_
#define PACKET_MANAGER_H_

#include "macro.h"
#include "config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    struct packet;
    typedef uintptr_t mv_pp;

    void mv_pp_init(mv_pp**);
    void mv_pp_destroy(mv_pp*);
    void mv_pp_ext(mv_pp*, int nworker);
    void mv_pp_free(mv_pp*, struct packet*);
    void mv_pp_free_to(mv_pp*, struct packet*, int pid);
    struct packet* mv_pp_alloc(mv_pp*, int pid);
    struct packet* mv_pp_alloc_nb(mv_pp*, int pid);

#ifdef __cplusplus
}
#endif

#endif
