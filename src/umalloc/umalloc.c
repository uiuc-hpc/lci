
/* Memory allocator `malloc'.
   Copyright 1990, 1991, 1992 Free Software Foundation

   Written May 1989 by Mike Haertel.
   Heavily modified Mar 1992 by Fred Fish for mmap'd version.

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

/* Prototypes for local functions */

static int initialize(struct mdesc *);
static void *morecore(struct mdesc *, uintptr_t);
static void *align(struct mdesc *, uintptr_t);

/* Aligned allocation.
 */

static void *align(struct mdesc *mdp, uintptr_t size)
{
    void *result;
    uintptr_t adj;

    if ((ptrdiff_t)size < 0)
	/* Size is so big that mdp->morecore will think it is negative. */
	return (NULL);

    result = mdp->morecore(mdp, size);
    adj = RESIDUAL(result, BLOCKSIZE);

    if (adj != 0) {
	if (MDP_GROWSUP(mdp)) {
	    adj = BLOCKSIZE - adj;
	    mdp->morecore(mdp, adj);
	    result = (char *) result + adj;
	}
	else 
	    result = mdp->morecore(mdp, adj);
    }
    return (result);
}

/* Set everything up and remember that we have.  */

static int initialize(struct mdesc *mdp)
{

    mdp->heapsize = HEAP / BLOCKSIZE;

#ifdef UMALLOC_EXTERNAL_METADATA
    mdp->heapinfo = (malloc_info *) 
        malloc(mdp->heapsize * sizeof(malloc_info));

    if (MDP_GROWSDOWN(mdp)) 
        mdp->heapbase = (void *)(((unsigned long)mdp->base + 1) & ~((unsigned long)BLOCKSIZE - 1));

    if (MDP_GROWSUP(mdp))
        mdp->heapbase = (char *) mdp->base;
#else

    if (MDP_GROWSDOWN(mdp)) 
        mdp->heapbase = (char *) align(mdp, 1);

    mdp->heapinfo = (malloc_info *) 
	align(mdp, mdp->heapsize * sizeof(malloc_info));

    if (MDP_GROWSUP(mdp))
        mdp->heapbase = (char *) mdp->heapinfo;
#endif

    if (mdp->heapinfo == NULL) {
	return (0);
    }
    memset((void *) mdp->heapinfo, 0, mdp->heapsize * sizeof(malloc_info));
    mdp->heapinfo[0].free.size = 0;
    mdp->heapinfo[0].free.next = mdp->heapinfo[0].free.prev = 0;
    mdp->heapindex = 0;
    mdp->flags |= MMALLOC_INITIALIZED;

    mdp->heapstats.chunks_used = 1;
    mdp->heapstats.bytes_used =
	BLOCKSIZE * BLOCKIFY(mdp->heapsize * sizeof(malloc_info));

    return (1);
}

/* Get neatly aligned memory, initializing or
   growing the heap info table as necessary. */

static void *morecore(struct mdesc *mdp, uintptr_t size)
{
    void *result;
    malloc_info *newinfo, *oldinfo;
    uintptr_t blockno, old_blockno;
    uintptr_t newsize;

    result = align(mdp, size);
    if (result == NULL) {
	return (NULL);
    }

    if (MDP_GROWSUP(mdp))
	blockno = BLOCK_UP((char *)result + size); /* 1 byte above highest managed location */
    else
	blockno = BLOCK_DOWN((char *)result - 1); /* 1 byte below lowest managed location */

    /* Check if we need to grow the info table.  */
    if (blockno > mdp->heapsize) {
	newsize = mdp->heapsize;
	while (blockno > newsize) {
	    newsize *= 2;
	}
#ifdef UMALLOC_EXTERNAL_METADATA
        mdp->heapinfo = realloc(mdp->heapinfo, newsize * sizeof(malloc_info));
        memset(mdp->heapinfo + mdp->heapsize, 0, 
               (newsize - mdp->heapsize)* sizeof(malloc_info));
#else
	newinfo = (malloc_info *) align(mdp, newsize * sizeof(malloc_info));
	if (newinfo == NULL) {

	    mdp->morecore(mdp, -size);
	    return (NULL);
	}
	memset((void *) newinfo, 0, newsize * sizeof(malloc_info));
	memcpy((void *) newinfo, (void *) mdp->heapinfo,
	       mdp->heapsize * sizeof(malloc_info));
	oldinfo = mdp->heapinfo;
	old_blockno = BLOCK_MDP(mdp, oldinfo);
	newinfo[old_blockno].busy.type = 0;
	newinfo[old_blockno].busy.info.size
	    = BLOCKIFY(mdp->heapsize * sizeof(malloc_info));
	mdp->heapstats.chunks_used++;
	mdp->heapstats.bytes_used +=
	    BLOCKSIZE * BLOCKIFY(newsize * sizeof(malloc_info));
	mdp->heapinfo = newinfo;
	__umalloc_free(mdp, (void *) oldinfo);
#endif
	mdp->heapsize = newsize;
    }

    mdp->heaplimit = MDP_GROWSUP(mdp) ? blockno : (blockno-1);
    return (result);
}

/* Allocate memory from the heap.  */

void *umalloc(umalloc_heap_t *md, uintptr_t size)
{
    struct mdesc *mdp;
    void *result;
    uintptr_t block, blocks, lastblocks, start;
    register uintptr_t i;
    struct list *next;
    register uintptr_t log;

    if (size == 0) {
	return (NULL);
    }

    mdp = MD_TO_MDP(md);

#if 0 /* We no longer use the debugging hooks */
    if (mdp->umalloc_hook != NULL) {
	return ((*mdp->umalloc_hook) (md, size));
    }
#endif

    if (!(mdp->flags & MMALLOC_INITIALIZED)) {
	if (!initialize(mdp)) {
	    return (NULL);
	}
    }

    if (size < sizeof(struct list)) {
	size = sizeof(struct list);
    }

    /* Determine the allocation policy based on the request size.  */
    if (size <= BLOCKSIZE / 2) {
	/* Small allocation to receive a fragment of a block.
	   Determine the logarithm to base two of the fragment size. */
	log = 1;
	--size;
	while ((size /= 2) != 0) {
	    ++log;
	}

	/* Look in the fragment lists for a
	   free fragment of the desired size. */
	next = mdp->fraghead[log].next;
	if (next != NULL) {
	    /* There are free fragments of this size.
	       Pop a fragment out of the fragment list and return it.
	       Update the block's nfree and first counters. */
	    result = (void *) next;
	    next->prev->next = next->next;
	    if (next->next != NULL) {
		next->next->prev = next->prev;
	    }
	    block = BLOCK_MDP(mdp, result);
	    if (--mdp->heapinfo[block].busy.info.frag.nfree != 0) {
		mdp->heapinfo[block].busy.info.frag.first =
		    RESIDUAL(next->next, BLOCKSIZE) >> log;
	    }

	    /* Update the statistics.  */
	    mdp->heapstats.chunks_used++;
	    mdp->heapstats.bytes_used += 1 << log;
	    mdp->heapstats.chunks_free--;
	    mdp->heapstats.bytes_free -= 1 << log;
	} else {
	    /* No free fragments of the desired size, so get a new block
	       and break it into fragments, returning the first.  */
	    result = umalloc(md, BLOCKSIZE);
	    if (result == NULL) {
		return (NULL);
	    }

	    /* Link all fragments but the first into the free list.  */
	    for (i = 1; i < (uintptr_t) (BLOCKSIZE >> log); ++i) {
		next = (struct list *) ((char *) result + (i << log));
		next->next = mdp->fraghead[log].next;
		next->prev = &mdp->fraghead[log];
		next->prev->next = next;
		if (next->next != NULL) {
		    next->next->prev = next;
		}
	    }

	    /* Initialize the nfree and first counters for this block.  */
	    block = BLOCK_MDP(mdp, result);
	    mdp->heapinfo[block].busy.type = log;
	    mdp->heapinfo[block].busy.info.frag.nfree = i - 1;
	    mdp->heapinfo[block].busy.info.frag.first = i - 1;

	    mdp->heapstats.chunks_free += (BLOCKSIZE >> log) - 1;
	    mdp->heapstats.bytes_free += BLOCKSIZE - (1 << log);
	    mdp->heapstats.bytes_used -= BLOCKSIZE - (1 << log);
	}
    } else {
	/* Large allocation to receive one or more blocks.
	   Search the free list in a circle starting at the last place visited.
	   If we loop completely around without finding a large enough
	   space we will have to get more memory from the system.  */
	blocks = BLOCKIFY(size);
	start = block = MALLOC_SEARCH_START;
	while (mdp->heapinfo[block].free.size < blocks) {
	    block = mdp->heapinfo[block].free.next;
	    if (block == start) {
		int   islast;
		void *newaddr;
		/* Need to get more from the system.  Check to see if
		   the new core will be contiguous with the final free
		   block; if so we don't need to get as much.  */
		block = mdp->heapinfo[0].free.prev;
		lastblocks = mdp->heapinfo[block].free.size;

		if (MDP_GROWSUP(mdp)) 
		    islast = block + lastblocks == mdp->heaplimit &&
			     mdp->morecore(mdp,0) == ADDRESS_UP(block+lastblocks);
		else
		    islast = block == mdp->heaplimit &&
			     mdp->morecore(mdp,0) == ADDRESS_DOWN(block);

		if (mdp->heaplimit != 0 && islast &&
		    (newaddr = morecore(mdp, (blocks-lastblocks)*BLOCKSIZE)) != NULL)
		{
		    /* Which block we are extending (the `final free
		       block' referred to above) might have changed, if
		       it got combined with a freed info table.  */
		    block = mdp->heapinfo[0].free.prev;

		    if (MDP_GROWSUP(mdp)) {
			mdp->heapinfo[block].free.size += (blocks - lastblocks);
		    }
		    else {
			/* We created a new 'lowest' block, so it must be
			 * updated in heapinfo[0]
			 */
			uintptr_t newblock = BLOCK_DOWN(newaddr);
			mdp->heapinfo[0].free.prev = newblock;
			mdp->heapinfo[newblock].free.size = blocks;
			mdp->heapinfo[newblock].free.prev =
				mdp->heapinfo[block].free.prev;
			mdp->heapinfo[newblock].free.next = 0;
			block = newblock;
		    }

		    mdp->heapstats.bytes_free +=
			(blocks - lastblocks) * BLOCKSIZE;
		    continue;
		}
		result = morecore(mdp, blocks * BLOCKSIZE);
		if (result == NULL) {
		    return (NULL);
		}
		block = BLOCK_MDP(mdp, result);
		mdp->heapinfo[block].busy.type = 0;
		mdp->heapinfo[block].busy.info.size = blocks;
		mdp->heapstats.chunks_used++;
		mdp->heapstats.bytes_used += blocks * BLOCKSIZE;
		return (result);
	    }
	}

	/* At this point we have found a suitable free list entry.
	   Figure out how to remove what we need from the list. */
	if (mdp->heapinfo[block].free.size > blocks) {
	    uintptr_t newblock;
	    /* The block we found has a bit left over,
	       so relink the tail end back into the free list. */
	    if (MDP_GROWSUP(mdp)) {
		newblock = block + blocks;

		mdp->heapinfo[newblock].free.size
		    = mdp->heapinfo[block].free.size - blocks;

		mdp->heapinfo[newblock].free.next
		    = mdp->heapinfo[block].free.next;
		mdp->heapinfo[newblock].free.prev
		    = mdp->heapinfo[block].free.prev;

		mdp->heapinfo[mdp->heapinfo[block].free.prev].free.next
		    = mdp->heapinfo[mdp->heapinfo[block].free.next].free.prev
		    = mdp->heapindex = newblock;
	    }
	    else {
		/* The existing free block loses some of its blocks */
		newblock = block;
		mdp->heapinfo[newblock].free.size -= blocks;
		block = newblock - mdp->heapinfo[newblock].free.size;
		mdp->heapindex = newblock;
	    }
	} else {
	    /* The block exactly matches our requirements,
	       so just remove it from the list. */
	    mdp->heapinfo[mdp->heapinfo[block].free.next].free.prev
		= mdp->heapinfo[block].free.prev;
	    mdp->heapinfo[mdp->heapinfo[block].free.prev].free.next
		= mdp->heapindex = mdp->heapinfo[block].free.next;
	    mdp->heapstats.chunks_free--;
	}

	result = ADDRESS_MDP(mdp, block);
	mdp->heapinfo[block].busy.type = 0;
	mdp->heapinfo[block].busy.info.size = blocks;
	mdp->heapstats.chunks_used++;
	mdp->heapstats.bytes_used += blocks * BLOCKSIZE;
	mdp->heapstats.bytes_free -= blocks * BLOCKSIZE;
    }

    return (result);
}


