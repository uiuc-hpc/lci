/* Copyright (c) 2001-2022, The Ohio State University. All rights
 * reserved.
 *
 * This file is part of the MVAPICH2 software package developed by the
 * team members of The Ohio State University's Network-Based Computing
 * Laboratory (NBCL), headed by Professor Dhabaleswar K. (DK) Panda.
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level MVAPICH2 directory.
 *
 */

#ifndef _MEM_HOOKS_H
#define _MEM_HOOKS_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "lcii.h"

typedef int (*munmap_t)(void*, size_t);

typedef struct {
//    int         is_our_malloc;
//    int         is_our_free;
//    int         is_our_calloc;
//    int         is_our_realloc;
//    int         is_our_valloc;
//    int         is_our_memalign;
    int         is_inside_free;
    int         is_mem_hook_finalized;
    munmap_t    munmap;
} mvapich2_malloc_info_t;

extern mvapich2_malloc_info_t mvapich2_minfo;

void mvapich2_mem_unhook(void *mem, size_t size);
int  mvapich2_minit(void);
void mvapich2_mfin(void);

int mvapich2_munmap(void *buf, size_t len);

#ifndef DISABLE_TRAP_SBRK
void *mvapich2_sbrk(intptr_t delta);
#endif /* DISABLE_TRAP_SBRK */

#endif /* _MEM_HOOKS_H */
