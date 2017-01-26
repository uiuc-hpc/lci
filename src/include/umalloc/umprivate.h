
/* Declarations for `umalloc' and friends.
   Copyright 1990, 1991, 1992 Free Software Foundation

   Written May 1989 by Mike Haertel.
   Heavily modified Mar 1992 by Fred Fish. (fnf@cygnus.com)

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#ifndef __UMPRIVATE_H
#define __UMPRIVATE_H 1

#ifndef _UMALLOC_INTERNAL
#error Include/build error - umprivate.h should only be included by umalloc implementation .c files
#endif

#include "umalloc.h"
#if defined(PPL_COMM_USE_GASNET)
#include "portable_platform.h" /* For PLATFORM_COMPILER_*, etc. */
#else
#include <stdint.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#else
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
#endif
#if defined(__PGI) && !defined(__CHAR_BIT__)
/* workaround a stupid bug in the PGI Linux headers */
#define __CHAR_BIT__ 8
#endif

/* Propagate the non-debug assertion through assert.h if we aren't debugging */
#ifndef PPL_DEBUG
#define NDEBUG 1
#endif

#include <assert.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

#define MMALLOC_MAGIC "umalloc" /* Mapped file magic number */
#define MMALLOC_MAGIC_SIZE 8    /* Size of magic number buf */
#define MMALLOC_VERSION 2       /* Current umalloc version */
#define MMALLOC_KEYS 16         /* Keys for application use */

/* The allocator divides the heap into blocks of fixed size; large
   requests receive one or more whole blocks, and small requests
   receive a fragment of a block.  Fragment sizes are powers of two,
   and all fragments of a block are the same size.  When all the
   fragments in a block have been freed, the block itself is freed.  */

#define INT_BIT (CHAR_BIT * sizeof(int))
#define BLOCKLOG (INT_BIT > 16 ? 12 : 9)
#define BLOCKSIZE ((unsigned int)1 << BLOCKLOG)
#define BLOCKIFY(SIZE) (((SIZE) + BLOCKSIZE - 1) / BLOCKSIZE)

/* The difference between two pointers is a signed int.  On machines where
   the data addresses have the high bit set, we need to ensure that the
   difference becomes an unsigned int when we are using the address as an
   integral value.  In addition, when using with the '%' operator, the
   sign of the result is machine dependent for negative values, so force
   it to be treated as an unsigned int. */

#define RESIDUAL(addr, bsize) (((uintptr_t)(addr)) % (uintptr_t)(bsize))
#define RESIDUAL_UP(addr, bsize) (((uintptr_t)(addr)) % (bsize))
#define RESIDUAL_DOWN(addr, bsize) \
  ((uintptr_t)(bsize) - ((uintptr_t)(addr)) % (bsize))

/* Determine the amount of memory spanned by the initial heap table
   (not an absolute limit).  */

#define HEAP (INT_BIT > 16 ? 4194304 : 65536)

/* Number of contiguous free blocks allowed to build up at the end of
   memory before they will be returned to the system.  */

#define FINAL_FREE_BLOCKS 8

/* Where to start searching the free list when looking for new memory.
   The two possible values are 0 and heapindex.  Starting at 0 seems
   to reduce total memory usage, while starting at heapindex seems to
   run faster.  */

#define MALLOC_SEARCH_START 0 /* OR mdp -> heapindex */

/* Address to block number and vice versa.  */

#define BLOCK_UP(A) (((char*)(A)-mdp->heapbase) / BLOCKSIZE + 1)
#define BLOCK_DOWN(A) ((mdp->heapbase - (char*)(A)-1) / BLOCKSIZE + 1)

#define ADDRESS_UP(B) ((void*)(((B)-1) * BLOCKSIZE + mdp->heapbase))
#define ADDRESS_DOWN(B) ((void*)(mdp->heapbase - (B)*BLOCKSIZE))

/* Data structure giving per-block information.  */

typedef union {
  /* Heap information for a busy block.  */
  struct {
    /* Zero for a large block, or positive giving the
       logarithm to the base two of the fragment size.  */
    int type;
    union {
      struct {
        uintptr_t nfree; /* Free fragments in a fragmented block.  */
        uintptr_t first; /* First free fragment of the block.  */
      } frag;
      /* Size (in blocks) of a large cluster.  */
      uintptr_t size;
    } info;
  } busy;
  /* Heap information for a free block (that may be the first of
     a free cluster).  */
  struct {
    uintptr_t size; /* Size (in blocks) of a free cluster.  */
    uintptr_t next; /* Index of next free cluster.  */
    uintptr_t prev; /* Index of previous free cluster.  */
  } free;
} malloc_info;

/* Hash table entry for blocks allocated with `umemalign' (or `uvalloc').  */

struct alignlist {
  struct alignlist* next;
  void* aligned; /* The address that umemaligned returned.  */
  void* exact;   /* The address that malloc returned.  */
};

/* Doubly linked lists of free fragments.  */

struct list {
  struct list* next;
  struct list* prev;
};

/* Statistics available to the user.
   By design, the internals of the malloc package are no longer
   exported to the user via an include file, so access to this data needs
   to be via some other mechanism, such as umstats_<something> where the
   return value is the <something> the user is interested in. */

struct mstats {
  uintptr_t bytes_total;     /* Total size of the heap. */
  uintptr_t chunks_used;     /* Chunks allocated by the user. */
  uintptr_t bytes_used;      /* Byte total of user-allocated chunks. */
  uintptr_t chunks_free;     /* Chunks in the free list. */
  uintptr_t bytes_free;      /* Byte total of chunks in the free list. */
  uintptr_t extra_corespace; /* Unallocated bytes available beyond current brk
                                point */
};

/* Internal structure that defines the format of the malloc-descriptor.
   This gets written to the base address of the region that umalloc is
   managing, and thus also becomes the file header for the mapped file,
   if such a file exists. */

struct mdesc {
  /* The size in bytes of this structure, used as a sanity check when reusing
     a previously created mapped file. */
  unsigned int headersize;

  /* The version number of the umalloc package that created this file. */
  unsigned char version;

  /* Some flag bits to keep track of various internal things. */
  unsigned int flags;

  /* If a system call made by the umalloc package fails, the errno is
     preserved for future examination. */
  int saved_errno;

  /* Pointer to the function that is used to get more core */
  void* (*morecore)(struct mdesc*, ptrdiff_t);

  /* hint to the client - holds last failed morecore request sz */
  uintptr_t morecore_hint;

  /* Pointer to the function that causes an abort when the memory checking
     features are activated.  By default this is set to abort(), but can
     be set to another function by the application using umalloc().
     FIXME:  For mapped regions shared by more than one process, this
     needs to be maintained on a per-process basis. */
  void (*abortfunc)(void);

  /* Debugging hook for free.
     FIXME:  For mapped regions shared by more than one process, this
     needs to be maintained on a per-process basis. */
  void (*ufree_hook)(void*, void*);

  /* Debugging hook for `malloc'.
     FIXME:  For mapped regions shared by more than one process, this
     needs to be maintained on a per-process basis. */
  void* (*umalloc_hook)(void*, uintptr_t);

  /* Debugging hook for realloc.
     FIXME:  For mapped regions shared by more than one process, this
     needs to be maintained on a per-process basis. */
  void* (*urealloc_hook)(void*, void*, uintptr_t);

  /* Number of info entries.  */
  uintptr_t heapsize;

  /* Pointer to first block of the heap (base of the first block).  */
  char* heapbase;

  /* Current search index for the heap table.  */
  /* Search index in the info table.  */
  uintptr_t heapindex;

  /* Limit of valid info table indices.  */
  uintptr_t heaplimit;

  /* Block information table.
     Allocated with malign/__umalloc_free (not umalloc/ufree).  */
  /* Table indexed by block number giving per-block information.  */
  malloc_info* heapinfo;

  /* Instrumentation.  */
  struct mstats heapstats;

  /* Free list headers for each fragment size.  */
  /* Free lists for each fragment size.  */
  struct list fraghead[BLOCKLOG];

  /* Hash table of blocks allocated by memalign.  */
  struct alignlist** aligned_block_table;
  uintptr_t aligned_block_tablecnt;
  uintptr_t aligned_block_tablesz;
  struct alignlist* aligned_freelist;

  /* The base address of the memory region for this malloc heap.  This
     is the location where the bookkeeping data for mmap and for malloc
     begins. */
  char* base;

  /* The current location in the memory region for this malloc heap which
     represents the end of memory in use. */
  char* breakval;

  /* The end of the current memory region for this malloc heap.  This is
     the first location past the end of mapped memory. */
  char* top;

  /* An array of keys to data within the mapped region, for use by the
     application.  */
  void* keys[MMALLOC_KEYS];
};

/* Bits to look at in the malloc descriptor flags word */

#define MMALLOC_DEVZERO (1 << 0)      /* Have mapped to /dev/zero */
#define MMALLOC_INITIALIZED (1 << 1)  /* Initialized umalloc */
#define MMALLOC_MMCHECK_USED (1 << 2) /* umcheckf() called already */
#define MMALLOC_GROWS_UP (1 << 3)     /* Heap grows upwards */

/* Internal version of `ufree' used in `morecore'. */
extern void __umalloc_free(struct mdesc*, void*);

/* insert an aligned entry into the table and return non-zero on success */
int __umalloc_aligned_insert(struct mdesc* mdp, void* exact, void* aligned);
/* remove an aligned entry from the table and return the exact value
 * (or null if not found in table) */
void* __umalloc_aligned_remove(struct mdesc* mdp, void* aligned);

/* Hooks for debugging versions.  */
extern void (*__ufree_hook)(void*, void*);
extern void* (*__umalloc_hook)(void*, uintptr_t);
extern void* (*__urealloc_hook)(void*, void*, uintptr_t);

/* Returns new chunk of given address space, or NULL if no more room */
extern void* __umalloc_mmap_morecore(struct mdesc*, int);

/* Macro to convert from a user supplied malloc descriptor to pointer to the
   internal malloc descriptor.  If the user supplied descriptor is NULL, then
   use the default internal version, initializing it if necessary.  Otherwise
   just cast the user supplied version (which is void *) to the proper type
   (struct mdesc *). */

#define MD_TO_MDP(md) (assert((md) != NULL), (struct mdesc*)(md))

#define MDP_GROWSUP(mdp) ((mdp)->flags & MMALLOC_GROWS_UP)
#define MDP_GROWSDOWN(mdp) !MDP_GROWSUP(mdp)
#define BLOCK_MDP(mdp, A) (MDP_GROWSUP(mdp) ? BLOCK_UP(A) : BLOCK_DOWN(A))
#define ADDRESS_MDP(mdp, B) (MDP_GROWSUP(mdp) ? ADDRESS_UP(B) : ADDRESS_DOWN(B))
#define RESIDUAL_MDP(mdp, addr, bsize) \
  (MDP_GROWSUP(mdp) ? RESIDUAL_UP(addr, bsize) : RESIDUAL_DOWN(addr, bsize))

#endif /* __UMPRIVATE_H */
