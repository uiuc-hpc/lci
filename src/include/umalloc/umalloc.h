#ifndef UMALLOC_H
#define UMALLOC_H 1

#if defined(PPL_COMM_USE_GASNET)
#include "portable_inttypes.h" /* need uintptr_t */
#else
#include <stdint.h>
#endif
#include <stddef.h> /* ptrdiff_t */

#define UMALLOC_HEAP_GROWS_DOWN 0
#define UMALLOC_HEAP_GROWS_UP 1

/* Heap type.  Never try to dereference this--just receive from umalloc_attach,
 * and pass to the various functions. */
struct umalloc_heap;
typedef struct umalloc_heap umalloc_heap_t;

/* Create a new heap with the given address and length
 * -- Memory must already be safe for access (mmapped, or otherwise made legal
 *    for use) */
extern umalloc_heap_t* umalloc_makeheap(void* addr, uintptr_t len,
                                        int heapgrows);

/* Give more address to a heap.
 * -- Client is in charge of allocating/mmapping the given addresses so that
 *    they are safe to hand out */
extern void umalloc_provide_pages(umalloc_heap_t* heap, uintptr_t len);

/* Allocate SIZE bytes of memory from heap.  */
extern void* umalloc(umalloc_heap_t* heap, uintptr_t size);

/* Re-allocate the previously allocated block in void *, making the new block
   SIZE bytes long.  */
extern void* urealloc(umalloc_heap_t* heap, void*, uintptr_t);

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
extern void* ucalloc(umalloc_heap_t* heap, uintptr_t, uintptr_t);

/* Free a block allocated by `umalloc', `urealloc' or `ucalloc'.  */
extern void ufree(umalloc_heap_t* heap, void*);

/* Allocate SIZE bytes allocated to ALIGNMENT bytes.  */
extern void* umemalign(umalloc_heap_t* heap, uintptr_t, uintptr_t);

/* Allocate SIZE bytes on a page boundary.  */
extern void* uvalloc(umalloc_heap_t* heap, uintptr_t);

/* Activate a standard collection of debugging hooks.  */
extern int umcheck(umalloc_heap_t* heap, void (*)(void));

extern int umcheckf(umalloc_heap_t* heap, void (*)(void), int);

#if 0
/* Pick up the current statistics. */
extern struct mstats umstats(umalloc_heap_t *heap);
#endif

extern uintptr_t umstats_bytes_total(umalloc_heap_t* md);
extern uintptr_t umstats_bytes_used(umalloc_heap_t* md);
extern uintptr_t umstats_bytes_free(umalloc_heap_t* md);
extern uintptr_t umstats_chunks_used(umalloc_heap_t* md);
extern uintptr_t umstats_chunks_free(umalloc_heap_t* md);
extern uintptr_t umstats_extra_corespace(umalloc_heap_t* md);

extern int umalloc_setkey(umalloc_heap_t*, int, void*);
extern void* umalloc_getkey(umalloc_heap_t*, int);

/*extern int umalloc_errno(void *); */

extern int umtrace(void);

/* Top of heap, required by UPCRuntime */
extern void* umalloc_topofheap(umalloc_heap_t* heap);

/* hint about size of last failed attempt to expand heap */
extern uintptr_t umalloc_morecore_hint(umalloc_heap_t* heap);

#endif /* UMALLOC_H */
