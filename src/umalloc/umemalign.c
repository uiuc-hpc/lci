/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
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

void *
umemalign (umalloc_heap_t *md, uintptr_t alignment, uintptr_t size)
{
  void *exact;
  void *aligned;
  uintptr_t adj;

  if ((exact = umalloc(md, size + alignment - 1)) == NULL) return NULL;
  else {
    adj = RESIDUAL(exact, alignment);
    if (adj == 0) return exact;
    else {
      aligned = (char *) exact + alignment - adj;
      if (!__umalloc_aligned_insert(MD_TO_MDP(md), exact, aligned)) {
        ufree(md, exact);
        return NULL;
      }
      return aligned;
    }
  }
  abort();
}

#define INITIAL_TABLE_SZ  63
/* insert an aligned entry into the table and return non-zero on success */
int __umalloc_aligned_insert(struct mdesc *mdp, void *exact, void *aligned) {
  umalloc_heap_t *md = (umalloc_heap_t *)mdp;

  /* fetch a list cell to use */
  struct alignlist *newl = mdp->aligned_freelist;
  if (newl) mdp->aligned_freelist = newl->next;
  else if ((newl = (struct alignlist *)
        umalloc(md, sizeof(struct alignlist))) == NULL) return 0;
  newl->aligned = aligned;
  newl->exact = exact;

  if (mdp->aligned_block_table == NULL) { /* first call - create table */
    mdp->aligned_block_table = ucalloc(md, INITIAL_TABLE_SZ, sizeof(struct alignlist*));
    if (mdp->aligned_block_table == NULL) {
      ufree(md, newl);
      return 0;
    }
    mdp->aligned_block_tablecnt = 0;
    mdp->aligned_block_tablesz = INITIAL_TABLE_SZ;
  } else if (mdp->aligned_block_tablecnt >= mdp->aligned_block_tablesz) { /* rehash */
    uintptr_t newtablesz = mdp->aligned_block_tablesz*2 + 1;
    uintptr_t i;
    struct alignlist **newtable = ucalloc(md, newtablesz, sizeof(struct alignlist*));
    if (newtable == NULL) {
      ufree(md, newl);
      return 0;
    }
    for (i=0; i < mdp->aligned_block_tablesz; i++) {
      struct alignlist *cur = mdp->aligned_block_table[i];
      while (cur) {
        struct alignlist *next = cur->next;
        uintptr_t key = ((uintptr_t)(cur->aligned))%newtablesz;
        cur->next = newtable[key];
        newtable[key] = cur;
        cur = next;
      }
    }
    { void *oldtable = mdp->aligned_block_table;
      mdp->aligned_block_table = newtable;
      mdp->aligned_block_tablesz = newtablesz;
      ufree(md, oldtable);
    }
  }

  /* insert */
  { uintptr_t key = ((uintptr_t)aligned)%mdp->aligned_block_tablesz;
    newl->next = mdp->aligned_block_table[key];
    mdp->aligned_block_table[key] = newl;
    mdp->aligned_block_tablecnt++;
  }
  return 1;
}
/* remove an aligned entry from the table and return the exact value
 * (or null if not found in table)
 */
void *__umalloc_aligned_remove(struct mdesc *mdp, void *aligned) {
  if (mdp->aligned_block_table == NULL) return NULL; /* no table */
  else {
    uintptr_t key = ((uintptr_t)aligned)%mdp->aligned_block_tablesz;
    struct alignlist **l = &(mdp->aligned_block_table[key]);
    for ( ; *l != NULL; l = &((*l)->next)) {
      if ((*l)->aligned == aligned) { /* found it - remove */
        struct alignlist *tmp = *l;
        *l = tmp->next;
        tmp->next = mdp->aligned_freelist;
        mdp->aligned_freelist = tmp;
        mdp->aligned_block_tablecnt--;
        return tmp->exact;
      }
    }
    return NULL;
  }
}
