/** <plaintext>
*
*  avl.h -- public types and external declarations for avl trees
*
*  Created 03/01/89 by Brad Appleton
*
* ^{Mods:* }
*
* Fri Jul 14 13:54:12 1989, Rev 1.0, brad(0165)
*
**/

/* Copyright (c) 2001-2016, The Ohio State University. All rights
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

#ifndef AVL_H
#define AVL_H

#ifndef NEMESIS_BUILD
// #include "mpichconf.h"
#endif

#if defined(__STDC__)
# define _P(x)  x
#else /* defined(__STDC__) */
# define _P(x)  ()
#endif /* defined(__STDC__) */

#include <stdlib.h>
#include <string.h>

       /* definition of traversal type */
typedef  enum  { PREORDER, INORDER, POSTORDER }  VISIT;


       /* definition of sibling order type */
typedef  enum  { LEFT_TO_RIGHT, RIGHT_TO_LEFT }  SIBLING_ORDER;


       /* definition of node type */
typedef  enum  { IS_TREE, IS_LBRANCH, IS_RBRANCH, IS_LEAF, IS_NULL }  NODE;


       /* definition of opaque type for AVL trees */
typedef  void  *AVL_TREE;


#if !defined(NEXTERN)

     /* Constructor and Destructor functions for AVL trees:
     *          avlfree is a macro for avldispose in the fashion
     *          of free(). It assumes certain default values
     *          (shown below) for the deallocation function and
     *          for the order in which children are traversed.
     */
extern AVL_TREE     avlinit    _P((long(*) (), unsigned long (*)()));
extern void         avldispose _P((AVL_TREE *, void(*) (), SIBLING_ORDER));
#define avlfree(x)  avldispose _P(&(x), free, LEFT_TO_RIGHT)


       /* Routine for manipulating/accessing each data item in a tree */
extern void      avlwalk  _P((AVL_TREE, void(*) (), SIBLING_ORDER));


       /* Routine for obtaining the size of an AVL tree */
extern long       avlcount  _P((AVL_TREE));


       /* Routines to search for a given item */
extern void     *avlins  _P((void *, AVL_TREE));
extern void     *avldel  _P((void *, AVL_TREE));
extern void     *avlfind _P((void *, AVL_TREE));
extern void     *avlfindex _P((long(*) (), void *, AVL_TREE));


       /* Routines to search for the minimal item of a tree */
extern void     *avldelmin  _P((AVL_TREE));
extern void     *avlfindmin _P((AVL_TREE));


       /* Routines to search for the maximal item of a tree */
extern void     *avldelmax  _P((AVL_TREE));
extern void     *avlfindmax _P((AVL_TREE));

#endif /* !defined(NEXTERN) */

#undef _P
#endif /* AVL_H */
