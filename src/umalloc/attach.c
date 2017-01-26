
/* Initialization for access to a mmap'd malloc managed region.
   Copyright 1992, 2000 Free Software Foundation, Inc.

   Contributed by Fred Fish at Cygnus Support.   fnf@cygnus.com

This file is part of the GNU C Library.

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
Boston, MA 02111-1307, USA.  */

#define _UMALLOC_INTERNAL
#include "umprivate.h"

#ifndef SEEK_SET
#define SEEK_SET 0
#endif


/*  
 *  Hands out address if there's space left on heap, or returns NULL.  
 *  -- our allocation function calls umalloc_provide_pages()
 *     when NULL is returned.
 */

static void *get_morecore_growup(struct mdesc *mdp, ptrdiff_t size)
{
   void *result = NULL;

   if (size > 0) {
       /* We are allocating memory. */
       if (mdp->breakval + size > mdp->top) {
	   /* don't have enough space */
           mdp->morecore_hint = ((mdp->breakval + size) - mdp->top);
	   return NULL;
       } else {
	   result = (void *) mdp->breakval;
	   mdp->breakval += size;
       }
   } else if (size == 0) {
       /* Just return the current "break" value. */
       result = mdp->breakval;
   } else if (size < 0) {
       /* If allocation requires also allocating more block infos,
	* and the latter fails, this codepath is used to undo the initial
	* allocation.
	*/
       mdp->breakval += size;   /* size is negative, so this rolls back breakval */
       assert(mdp->breakval >= mdp->heapbase);
       result = NULL;
   }
   return (result);
}


/*  
 * Same as growup, for heaps growing down.
 */

static void *get_morecore_growdown(struct mdesc *mdp, ptrdiff_t size)
{
   void *result = NULL;

   if (size > 0) {
       /* We are allocating memory. */
       if (mdp->breakval - size < mdp->top) {
	   /* don't have enough space */
           mdp->morecore_hint = (mdp->top - (mdp->breakval - size));
	   return NULL;
       } else {
	   /* grow down: move breakval from 'size' first */
	   mdp->breakval -= size;
	   result = (void *) mdp->breakval;
       }
   } else if (size == 0) {
       /* Just return the current "break" value. */
       result = mdp->breakval;
   } else if (size < 0) {
       /* If allocation requires also allocating more block infos,
	* and the latter fails, this codepath is used to undo the initial
	* allocation.
	*/
       mdp->breakval -= size;   /* size is negative, so this rolls back breakval */
       assert(mdp->breakval <= mdp->heapbase);
       result = NULL;
   }
   return (result);
}


/* Initialize access to a umalloc managed region.  
 *
 * Takes the address and length of a segment which the heap is allowed to assign
 * addresses from.  This region must already be legal--i.e. it has been mmapped
 * or otherwise made into a region of legal addresses that are safe to access
 * without segfaulting.  
 *
 * On success, returns a "heap descriptor" which is used in subsequent calls
 * to other umalloc package functions.  It is an opaque struct, which should
 * never be dereferenced (you'll get a compiler error if you try).
 *
 * On failure returns NULL. 
 */

umalloc_heap_t * umalloc_makeheap(void *baseaddr, uintptr_t len, int heapgrows)
{
    struct mdesc mtemp;
    struct mdesc *mdp;
    void *mbase;

    /* We start off with the malloc descriptor allocated on the stack we
     * allocate the first page of the region and copy it there.  Ensure that it
     * is zero'd and then initialize the fields that we know values for. 
     */
    mdp = &mtemp;
    memset((char *) mdp, 0, sizeof(mtemp));
    mdp->headersize = sizeof(mtemp);
    mdp->version = MMALLOC_VERSION;

    assert ((len & (BLOCKSIZE-1)) == 0);
    if (heapgrows == UMALLOC_HEAP_GROWS_UP) {
        assert ((((uintptr_t)baseaddr) & (BLOCKSIZE-1)) == 0);
	mdp->base     = baseaddr;
	mdp->breakval = baseaddr;
	mdp->top      = ((char*)baseaddr) + len; 
	mdp->flags   |= MMALLOC_GROWS_UP;
	mdp->morecore = get_morecore_growup;
    }
    else {
	/* make sure we don't wrap around */
	assert((uintptr_t)baseaddr+1 != 0);
	mdp->base     = baseaddr;
	/* breakval points to a non-mapped address, but this simplifies
	 * arithmetic.  We make sure not to use this address. */
	mdp->breakval = (char *)baseaddr+1;
	mdp->top      = ((char*)baseaddr) - len + 1; 
        assert ((((uintptr_t)mdp->top) & (BLOCKSIZE-1)) == 0);
	mdp->morecore = get_morecore_growdown;
    }

    /* Copy the malloc descriptor structure into the region, and arrange to
     * return a pointer to this new copy.  If there is not enough room in the
     * area for the heap struct, return NULL.
     */

#ifdef UMALLOC_EXTERNAL_METADATA
    if (((mbase = malloc(sizeof(mtemp))) != NULL)) {
#else
    if ((mbase = mdp->morecore(mdp, sizeof(mtemp))) != NULL) {
#endif
	memcpy(mbase, mdp, sizeof(mtemp));
	mdp = (struct mdesc *) mbase;
    } else {
	mdp = NULL;
    }

    return ((umalloc_heap_t *) mdp);
}


/* 
 * Give more address to a heap.  
 *
 * Client is in charge of allocating/mmapping the given addresses so that they
 * are safe.
 */
void umalloc_provide_pages(umalloc_heap_t *heap, uintptr_t len)
{
    struct mdesc *mdp;

    assert(len > 0);

    mdp = MD_TO_MDP(heap);
    if (mdp->flags & MMALLOC_GROWS_UP)
	mdp->top += len;
    else
	mdp->top -= len;
    mdp->morecore_hint = 0;
}

void *
umalloc_topofheap(umalloc_heap_t *heap)
{
    struct mdesc *mdp = MD_TO_MDP(heap);
    return (void*) mdp->top;
}

uintptr_t umalloc_morecore_hint(umalloc_heap_t *heap) {
    struct mdesc *mdp = MD_TO_MDP(heap);
    return mdp->morecore_hint;
}
