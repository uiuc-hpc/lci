
/* Access the statistics maintained by `umalloc'.
   Copyright 1990, 1991, 1992 Free Software Foundation

   Written May 1989 by Mike Haertel.
   Modified Mar 1992 by Fred Fish.  (fnf@cygnus.com)

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

#define _UMALLOC_INTERNAL
#include "umprivate.h"

static
struct mstats umstats(umalloc_heap_t *md)
{
    struct mstats result;
    struct mdesc *mdp;
    char *brk;

    mdp = MD_TO_MDP(md);
    brk = (char *) mdp->morecore(mdp, 0);
    if (MDP_GROWSUP(mdp)) {
      result.extra_corespace = mdp->top - brk;
      result.bytes_total = brk - mdp->heapbase;
    } else {
      result.extra_corespace = brk - mdp->top;
      result.bytes_total = mdp->heapbase - brk;
    }
    result.chunks_used = mdp->heapstats.chunks_used;
    result.bytes_used = mdp->heapstats.bytes_used;
    result.chunks_free = mdp->heapstats.chunks_free;
    result.bytes_free = mdp->heapstats.bytes_free;
    return (result);
}

uintptr_t umstats_extra_corespace(umalloc_heap_t *md) { return umstats(md).extra_corespace; }
uintptr_t umstats_bytes_total(umalloc_heap_t *md) { return umstats(md).bytes_total; }
uintptr_t umstats_bytes_used(umalloc_heap_t *md) { return umstats(md).bytes_used; }
uintptr_t umstats_bytes_free(umalloc_heap_t *md) { return umstats(md).bytes_free; }
uintptr_t umstats_chunks_used(umalloc_heap_t *md) { return umstats(md).chunks_used; }
uintptr_t umstats_chunks_free(umalloc_heap_t *md) { return umstats(md).chunks_free; }

