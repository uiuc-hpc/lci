/** <plaintext>
 *
 * avl.c -- C source file for avl trees. Contains the auxillary routines
 *          and defines for the avl tree functions and user interface and
 *          includes all the necessary public and private routines
 *
 * Created 03/01/89 by Brad Appleton
 *
 * ^{Mods:* }
 *
 * Fri Jul 14 13:53:42 1989, Rev 1.0, brad(0165)
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

#define __UNUSED__ __attribute__((unused))

#ifndef NEMESIS_BUILD
// #include "mpichconf.h"
#endif
// #include <mpiimpl.h>
// #include <mpimem.h>
#include <stdio.h>
#include "avl.h"      /* public types for avl trees */
#include "avl_typs.h" /* private types for avl trees */
#include "dreg.h"

/* some common #defines used throughout most of my files */
#define PUBLIC /* default */
#define PRIVATE static
#define FALSE 0
#define TRUE !FALSE

#define NEXTERN /* dont include "extern" declarations from header files */

#if !defined(DISABLE_PTMALLOC)

/* We cannot call "free" from within the AVL tree
 * implementation, since, this may have been
 * invoked from within a call to free hook. To
 * avoid calling free, we are keeping a freelist
 * of AVLtrees */

/* Notes: There are two free calls in this file
 * One, inside avl_delete (deals with AVLtree)
 * Second, inside avl_free. However, avl_free is
 * never called, since avldispose (parent function)
 * is never used.
 */

static AVLnode avl_free_list;

#define INIT_AVL_FREE_LIST(_list) \
  {                               \
    (_list)->next = NULL;         \
  }

#define ADD_AVL_FREE_LIST(_list, _v) \
  {                                  \
    (_v)->next = (_list)->next;      \
    (_list)->next = (_v);            \
  }

#define GET_AVL_FREE_LIST(_list, _v)                   \
  {                                                    \
    *(_v) = (_list)->next;                             \
    if ((_list)->next) {                               \
      (_list)->next = ((AVLnode*)(_list)->next)->next; \
    }                                                  \
  }

#endif /* !defined(DISABLE_PTMALLOC) */

/************************************************************************
 *       Auxillary functions
 *
 *       routines to allocate/de-allocate an AVL node,
 *       and determine the type of an AVL node.
 ************************************************************************/

/* ckalloc(size) -- allocate space; check for success */
PRIVATE
void* ckalloc(size_t size)
{
  char* ptr = malloc((size_t)size);

  if (ptr == NULL) {
    fprintf(stderr, "Unable to allocate storage.");
    exit(1);
  }

  return ptr;
} /* ckalloc */

/*
 * new_node() -- get space for a new node and its data;
 *               return the address of the new node
 */
PRIVATE AVLtree new_node(void* data, size_t size)
{
  AVLtree root;

#if !defined(DISABLE_PTMALLOC)
  GET_AVL_FREE_LIST(&avl_free_list, &root);

  if (NULL == root) {
    root = (AVLtree)ckalloc(sizeof(AVLnode));
    root->data = (void*)ckalloc(size);
  }

  root->next = NULL;
#else  /* !defined(DISABLE_PTMALLOC) */
  root = (AVLtree)ckalloc(sizeof(AVLnode));
  root->data = (void*)ckalloc(size);
#endif /* !defined(DISABLE_PTMALLOC) */
  memcpy(root->data, data, size);
  root->bal = BALANCED;
  root->subtree[LEFT] = root->subtree[RIGHT] = NULL_TREE;

  return root;
} /* new_node */

/*
 * free_node()  --  free space for a node and its data!
 *                  reset the node pointer to NULL
 */
PRIVATE void free_node(rootp) AVLtree* rootp;
{
#if !defined(DISABLE_PTMALLOC)
  if (g_is_dreg_finalize == 1) {
    free((*rootp)->data);
    free((void*)*rootp);
  } else {
    ADD_AVL_FREE_LIST(&avl_free_list, *rootp);
  }
#else  /* !defined(DISABLE_PTMALLOC) */
  free((*rootp)->data);
  free((void*)*rootp);
#endif /* !defined(DISABLE_PTMALLOC) */
  *rootp = NULL_TREE;
} /* free_node */

/*
 * node_type() -- determine the number of null pointers for a given
 *                node in an AVL tree, Returns a value of type NODE
 *                which is an enumeration type with the following values:
 *
 *                  IS_TREE     --  both subtrees are non-empty
 *                  IS_LBRANCH  --  left subtree is non-empty; right is empty
 *                  IS_RBRANCH  --  right subtree is non-empty; left is empty
 *                  IS_LEAF     --  both subtrees are empty
 *                  IS_NULL     --  given tree is empty
 */
PRIVATE NODE node_type(tree) AVLtree tree;
{
  if (tree == NULL_TREE) {
    return IS_NULL;
  } else if (tree->subtree[LEFT] != NULL_TREE &&
             tree->subtree[RIGHT] != NULL_TREE) {
    return IS_TREE;
  } else if (tree->subtree[LEFT] != NULL_TREE) {
    return IS_LBRANCH;
  } else if (tree->subtree[RIGHT] != NULL_TREE) {
    return IS_RBRANCH;
  }

  return IS_LEAF;
} /* node_type */

/************************************************************************
 *       PRIVATE functions for manipulating AVL trees
 *
 *  This following defines a set of routines for creating, maintaining, and
 *  manipulating AVL Trees as an Abtract Data Type. The routines in this
 *  file that are accessible (through the avl tree user-interface) to other
 *  files to allow other programmers to:
 *
 *       Insert, Delete, and Find a given data item from a Tree.
 *
 *       Delete and Find the minimal and Maximal items in a Tree.
 *
 *       Walk through every node in a tree performing a giving operation.
 *
 *       Walk through and free up space for every node in a tree while
 *performing a given operation on the data items as they are encountered.
 ************************************************************************/

/************************************************************************
 *       routines used to find the minimal and maximal elements
 *       (nodes) of an AVL tree.
 ************************************************************************/

/*
 * avl_min() -- compare function used to find the minimal element in a tree
 */
PRIVATE long avl_min(void* elt1 __UNUSED__, void* elt2 __UNUSED__, NODE nd_typ)
// void *elt1, *elt2;
// NODE  nd_typ;
{
  return (nd_typ == IS_RBRANCH || nd_typ == IS_LEAF)
             ? 0   /* left subtree is empty -- this is the minimum */
             : -1; /* keep going left */
} /* avl_min */

/*
 * avl_max() -- compare function used to find the maximal element in a tree
 */
PRIVATE long avl_max(void* elt1 __UNUSED__, void* elt2 __UNUSED__, NODE nd_typ)
// void *elt1, *elt2;
// NODE  nd_typ;
{
  return (nd_typ == IS_LBRANCH || nd_typ == IS_LEAF)
             ? 0  /* right subtree is empty -- this is the maximum */
             : 1; /* keep going right */
} /* avl_max */

/************************************************************************
 *       Routines to perform rotations on AVL trees
 ************************************************************************/

/*
 * rotate_once()  --  rotate a given node in the given direction
 *                    to restore the balance of a tree
 */
PRIVATE short rotate_once(rootp, dir) AVLtree* rootp;
DIRECTION dir;
{
  DIRECTION other_dir = OPPOSITE(dir); /* opposite direction to "dir" */
  AVLtree old_root = *rootp;           /* copy of original root of tree */
  short ht_unchanged;                  /* true if height unchanged */

  ht_unchanged = ((*rootp)->subtree[other_dir]->bal) ? FALSE : TRUE;

  /* assign new root */
  *rootp = old_root->subtree[other_dir];

  /* new-root exchanges it's "dir" subtree for it's parent */
  old_root->subtree[other_dir] = (*rootp)->subtree[dir];
  (*rootp)->subtree[dir] = old_root;

  /* update balances */
  old_root->bal = -(dir == LEFT ? --((*rootp)->bal) : ++((*rootp)->bal));

  return ht_unchanged;
} /* rotate_once */

/*
 * rotate_twice()  --  rotate a given node in the given direction
 *                     and then in the opposite direction
 *                     to restore the balance of a tree
 */
PRIVATE void rotate_twice(rootp, dir) AVLtree* rootp;
DIRECTION dir;
{
  DIRECTION other_dir = OPPOSITE(dir);
  AVLtree old_root = *rootp;
  AVLtree old_other_dir_subtree = (*rootp)->subtree[other_dir];

  /* assign new root */
  *rootp = old_root->subtree[other_dir]->subtree[dir];

  /* new-root exchanges it's "dir" subtree for it's grandparent */
  old_root->subtree[other_dir] = (*rootp)->subtree[dir];
  (*rootp)->subtree[dir] = old_root;

  /* new-root exchanges it's "other-dir" subtree for it's parent */
  old_other_dir_subtree->subtree[dir] = (*rootp)->subtree[other_dir];
  (*rootp)->subtree[other_dir] = old_other_dir_subtree;

  /* update balances */
  (*rootp)->subtree[LEFT]->bal = -MAX((*rootp)->bal, 0);
  (*rootp)->subtree[RIGHT]->bal = -MIN((*rootp)->bal, 0);
  (*rootp)->bal = 0;
} /* rotate_twice */

/************************************************************************
 *                       Rebalance an AVL tree
 ************************************************************************/

/*
 * balance()  --  determines and performs the  sequence of rotations needed
 *                   (if any) to restore the balance of a given tree.
 *
 *     Returns 1 if tree height changed due to rotation; 0 otherwise
 */
PRIVATE short balance(rootp) AVLtree* rootp;
{
  short special_case = FALSE;

  if (LEFT_IMBALANCE(*rootp)) { /* need a right rotation */
    if ((*rootp)->subtree[LEFT]->bal == RIGHT_HEAVY) {
      rotate_twice(rootp, RIGHT); /* double RL rotation needed */
    } else {                      /* single RR rotation needed */
      special_case = rotate_once(rootp, RIGHT);
    }
  } else if (RIGHT_IMBALANCE(*rootp)) { /* need a left rotation */
    if ((*rootp)->subtree[RIGHT]->bal == LEFT_HEAVY) {
      rotate_twice(rootp, LEFT); /* double LR rotation needed */
    } else {                     /* single LL rotation needed */
      special_case = rotate_once(rootp, LEFT);
    }
  } else {
    return (short)HEIGHT_UNCHANGED; /* no rotation occurred */
  }

  return (special_case) ? (short)HEIGHT_UNCHANGED : (short)HEIGHT_CHANGED;
} /* balance */

/************************************************************************
 *       Routines to:    Find an item in an AVL tree
 *                       Insert an item into an AVL tree
 *                       Delete an item from an AVL tree
 ************************************************************************/

/*
 * avl_find() -- find an item in the given tree
 *
 *   PARAMETERS:
 *                data       --  a pointer to the key to find
 *                rootp      --  a pointer to an AVL tree
 *                compar     --  name of a function to compare 2 data items
 */
PRIVATE void* avl_find(data, tree, compar) void* data;
AVLtree tree;
long (*compar)();
{
  NODE nd_typ = node_type(tree);
  long cmp;

  while ((tree != NULL_TREE) && (cmp = (*compar)(data, tree->data, nd_typ))) {
    tree = tree->subtree[(cmp < 0) ? LEFT : RIGHT];
  }

  return (tree == NULL_TREE) ? NULL : tree->data;
} /* avl_find */

/*
 * avl_insert() -- insert an item into the given tree
 *
 *   PARAMETERS:
 *                data       --  a pointer to a pointer to the data to add;
 *                               On exit, *data is NULL if insertion succeeded,
 *                               otherwise address of the duplicate key
 *                rootp      --  a pointer to an AVL tree
 *                compar     --  name of the function to compare 2 data items
 */
PRIVATE short avl_insert(data, size, rootp, compar) void** data;
unsigned long size;
AVLtree* rootp;
long (*compar)();
{
  short increase;
  long cmp;

  if (*rootp == NULL_TREE) { /* insert new node here */
    *rootp = new_node(*data, size);
    *data = NULL; /* set return value in data */
    return (short)HEIGHT_CHANGED;
  } /* if */

  cmp = (*compar)(*data, (*rootp)->data); /* compare data items */

  if (cmp < 0) { /* insert into the left subtree */
    increase = -avl_insert(data, size, &((*rootp)->subtree[LEFT]), compar);
    if (*data != NULL) {
      return (short)HEIGHT_UNCHANGED;
    }
  } else if (cmp > 0) { /* insert into the right subtree */
    increase = avl_insert(data, size, &((*rootp)->subtree[RIGHT]), compar);
    if (*data != NULL) {
      return (short)HEIGHT_UNCHANGED;
    }
  } else {                  /* data already exists */
    *data = (*rootp)->data; /* set return value in data */
    return (short)HEIGHT_UNCHANGED;
  }

  (*rootp)->bal += increase; /* update balance factor */

  /************************************************************************
   * re-balance if needed -- height of current tree increases only if its
   * subtree height increases and the current tree needs no rotation.
   ************************************************************************/
  return (increase && (*rootp)->bal) ? 1 - balance(rootp)
                                     : (short)HEIGHT_UNCHANGED;
} /* avl_insert */

/*
 * avl_smallest() -- find smallest element in a given tree
 *
 *  PARAMETERS:
 *              data       -- a pointer to a pointer to key searched
 *              rootp      -- a pointer to AVL tree
 */
PRIVATE AVLtree* avl_smallest(data, rootp) void** data;
AVLtree* rootp;
{
  AVLtree* smallest = NULL;
  long ret;

  while (1) {
    smallest = rootp;
    ret = avl_min(*data, (*rootp)->data, node_type(*rootp));
    if (0 == ret) {
      break;
    }
    rootp = &((*rootp)->subtree[LEFT]);
  }

  return smallest;
}

/*
 * avl_delete() -- delete an item from the given tree
 *
 *   PARAMETERS:
 *                data       --  a pointer to a pointer to the key to delete
 *                               On exit, *data points to the deleted data item
 *                               (or NULL if deletion failed).
 *                rootp      --  a pointer to an AVL tree
 *                compar     --  name of function to compare 2 data items
 */
PRIVATE short avl_delete(data, rootp, compar) void** data;
AVLtree* rootp;
long (*compar)();
{
  short decrease = 0;
  long cmp;
  AVLtree old_root = *rootp;
  AVLtree* successor;
  void* successor_data;
  void* current_data;
  char scratch_space[32];
  NODE nd_typ = node_type(*rootp);
  DIRECTION dir = (nd_typ == IS_LBRANCH) ? LEFT : RIGHT;

  if (*rootp == NULL_TREE) { /* data not found */
    *data = NULL;            /* set return value in data */
    return (short)HEIGHT_UNCHANGED;
  } /* if */

  cmp = compar(*data, (*rootp)->data, nd_typ); /* compare data items */
  if (cmp < 0) {                               /* delete from left subtree */
    decrease = -avl_delete(data, &((*rootp)->subtree[LEFT]), compar);
    if (*data == NULL) {
      return (short)HEIGHT_UNCHANGED;
    }
  } else if (cmp > 0) { /* delete from right subtree */
    decrease = avl_delete(data, &((*rootp)->subtree[RIGHT]), compar);
    if (*data == NULL) {
      return (short)HEIGHT_UNCHANGED;
    }
  } else {                  /* cmp == 0 */
    *data = (*rootp)->data; /* set return value in data */

    /***********************************************************************
     *  At this point we know "cmp" is zero and "*rootp" points to
     *  the node that we need to delete.  There are three cases:
     *
     *     1) The node is a leaf.  Remove it and return.
     *
     *     2) The node is a branch (has only 1 child). Make "*rootp"
     *        (the pointer to this node) point to the child.
     *
     *     3) The node has two children. We swap data with the successor of
     *        "*rootp" (the smallest item in its right subtree) and delete
     *        the successor from the right subtree of "*rootp".  The
     *        identifier "decrease" should be reset if the subtree height
     *        decreased due to the deletion of the successor of "rootp".
     ***********************************************************************/

    switch (nd_typ) { /* what kind of node are we removing? */
      case IS_LEAF:
        free_node(rootp);             /* free the leaf, its height     */
        return (short)HEIGHT_CHANGED; /* changes from 1 to 0, return 1 */
      case IS_RBRANCH:                /* only child becomes new root */
      case IS_LBRANCH:
        *rootp = (*rootp)->subtree[dir];
        free_node(&old_root);         /* free the deleted node */
        return (short)HEIGHT_CHANGED; /* we just shortened the "dir" subtree */
      case IS_TREE:
        /* Find the min. item in the right subtree */
        successor =
            avl_smallest(&((*rootp)->data), &((*rootp)->subtree[RIGHT]));
        successor_data = (*successor)->data;
        current_data = (*rootp)->data;

        /* We have to do a swap without calling free over
         * here to avoid having special code for PTMALLOC
         * case. So, just copy out the required data into
         * scratch space. Remember that scratch space is
         * only 32 bytes long, statically allocated to
         * avoid calling malloc from here, since we may
         * already be inside `free'. 32-bytes should be
         * OK for us, since our data size is max 8 bytes
         * long.
         */
        memcpy(scratch_space, successor_data, sizeof(void*));

        decrease =
            avl_delete(&((*rootp)->data), &((*rootp)->subtree[RIGHT]), avl_min);

        /* Restore the overwritten pointer caused by the
         * recursive search into the AVL tree */
        (*rootp)->data = current_data;

        /* Restore the data in the pointer from what
         * was freed */
        memcpy((*rootp)->data, scratch_space, sizeof(void*));

      default:
        break;
    } /* switch */
  }   /* else */

  (*rootp)->bal -= decrease; /* update balance factor */

  /**********************************************************************
   * Rebalance if necessary -- the height of current tree changes if one
   * of two things happens: (1) a rotation was performed which changed
   * the height of the subtree (2) the subtree height decreased and now
   * matches the height of its other subtree (so the current tree now
   * has a zero balance when it previously did not).
   **********************************************************************/
  if (decrease && (*rootp)->bal) {
    return balance(rootp); /* rebalance and see if height changed */
  } else if (decrease && !(*rootp)->bal) {
    return (short)HEIGHT_CHANGED; /* balanced because subtree decreased */
  }

  return (short)HEIGHT_UNCHANGED;
} /* avl_delete */

/**
 *       Routines which Recursively Traverse an AVL TREE
 *
 * These routines may perform a particular action function upon each node
 * encountered. In these cases, "action" has the following definition:
 *
 *   void action(data, order, node, level, bal)
 *       void   *data
 *       VISIT   order;
 *       NODE    node;
 *       short   bal;
 *       long	level;
 *
 *         "data"    is a pointer to the data field of an AVL node
 *         "order"   corresponds to whether this is the 1st, 2nd or 3rd time
 *                   that this node has been visited.
 *         "node"    indicates which children (if any) of the current node
 *                   are null.
 *         "level"   is the current level (or depth) in the tree of the
 *                   curent node.
 *         "bal"     is the balance factor of the current node.
 **/

/************************************************************************
 *       Walk an AVL tree, performing a given function at each node
 ************************************************************************/

/*
 * avl_walk -- traverse the given tree performing "action"
 *            upon each data item encountered.
 *
 */
PRIVATE void avl_walk(tree, action, sibling_order, level) AVLtree tree;
void (*action)();
SIBLING_ORDER sibling_order;
long level;
{
  DIRECTION dir1 = (sibling_order == LEFT_TO_RIGHT) ? LEFT : RIGHT;
  DIRECTION dir2 = OPPOSITE(dir1);
  NODE node = node_type(tree);

  if (tree != NULL_TREE && action != NULL_ACTION) {
    (*action)(tree->data, PREORDER, node, level, tree->bal);
    if (tree->subtree[dir1] != NULL_TREE) {
      avl_walk(tree->subtree[dir1], action, sibling_order, level + 1);
    }

    (*action)(tree->data, INORDER, node, level, tree->bal);

    if (tree->subtree[dir2] != NULL_TREE) {
      avl_walk(tree->subtree[dir2], action, sibling_order, level + 1);
    }

    (*action)(tree->data, POSTORDER, node, level, tree->bal);
  } /* if non-empty tree */
} /* avl_walk */

/************************************************************************
 *       Walk an AVL tree, de-allocating space for each node
 *       and performing a given function at each node
 *       (such as de-allocating the user-defined data item).
 ************************************************************************/

/*
 * avl_free() -- free up space for all nodes in a given tree
 *              performing "action" upon each data item encountered.
 *
 *       (only perform "action" if it is a non-null function)
 */
PRIVATE void avl_free(AVLtree* rootp, void (*action)(),
                      SIBLING_ORDER sibling_order, long level)
{
  DIRECTION dir1 = (sibling_order == LEFT_TO_RIGHT) ? LEFT : RIGHT;
  DIRECTION dir2 = OPPOSITE(dir1);
  NODE node = node_type(*rootp);

  if (*rootp != NULL_TREE) {
    if (action != NULL_ACTION) {
      (*action)((*rootp)->data, PREORDER, node, level);
    }

    if ((*rootp)->subtree[dir1] != NULL_TREE) {
      avl_free(&((*rootp)->subtree[dir1]), action, sibling_order, level + 1);
    }

    if (action != NULL_ACTION) {
      (*action)((*rootp)->data, INORDER, node, level);
    }

    if ((*rootp)->subtree[dir2] != NULL_TREE) {
      avl_free(&((*rootp)->subtree[dir2]), action, sibling_order, level + 1);
    }

    if (action != NULL_ACTION) {
      (*action)((*rootp)->data, POSTORDER, node, level);
    }

    free_node(rootp);
  } /* if non-empty tree */
} /* avl_free */

/**********************************************************************
 *
 *               C-interface (public functions) for avl trees
 *
 *       These are the functions that are visible to the user of the
 *       AVL Tree Library. Mostly they just return or modify a
 *       particular attribute, or Call a private functions with the
 *       given parameters.
 *
 *       Note that public routine names begin with "avl" whereas
 *       private routine names that are called by public routines
 *       begin with "avl_" (the underscore character is added).
 *
 *       Each public routine must convert (cast) any argument of the
 *       public type "AVL_TREE" to a pointer to on object of the
 *       private type "AVLdescriptor" before passing the actual
 *       AVL tree to any of the private routines. In this way, the
 *       type "AVL_TREE" is implemented as an opaque type.
 *
 *       An "AVLdescriptor" is merely a container for AVL-tree
 *       objects which contains the pointer to the root of the
 *       tree and the various attributes of the tree.
 *
 *       The function types prototypes for the routines which follow
 *       are declared in the include file "avl.h"
 *
 ***********************************************************************/

/*
 * avlinit() -- get space for an AVL descriptor for the given tree
 *              structure and initialize its fields.
 */
PUBLIC AVL_TREE avlinit(compar, isize) long (*compar)();
unsigned long (*isize)();
{
  AVLdescriptor* avl_desc;

#if !defined(DISABLE_PTMALLOC)
  INIT_AVL_FREE_LIST(&avl_free_list);
#endif /* !defined(DISABLE_PTMALLOC) */

  avl_desc = (AVLdescriptor*)ckalloc(sizeof(AVLdescriptor));
  avl_desc->root = NULL_TREE;
  avl_desc->compar = compar;
  avl_desc->isize = isize;
  avl_desc->count = 0;
  return (AVL_TREE)avl_desc;
} /* avlinit */

/*
 * avldispose() -- free up all space associated with the given tree structure.
 */
PUBLIC void avldispose(treeptr, action, sibling_order) AVL_TREE* treeptr;
void (*action)();
SIBLING_ORDER sibling_order;
{
  AVLdescriptor* avl_desc;

#if !defined(DISABLE_PTMALLOC)
  AVLtree root;

  GET_AVL_FREE_LIST(&avl_free_list, &root);
  while (NULL != root) {
    free(root->data);
    free(root);
    GET_AVL_FREE_LIST(&avl_free_list, &root);
  }
#endif

  avl_desc = (AVLdescriptor*)*treeptr;
  if (&(avl_desc->root) == NULL) {
    free(treeptr);
    return;
  }
  avl_free(&(avl_desc->root), action, sibling_order, 1);
  free(treeptr);
} /* avldispose */

/*
 * avlwalk() -- traverse the given tree structure and perform the
 *              given action on each data item in the tree.
 */
PUBLIC void avlwalk(AVL_TREE tree, void (*action)(),
                    SIBLING_ORDER sibling_order)
{
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  avl_walk(avl_desc->root, action, sibling_order, 1);
} /* avlwalk */

/*
 * avlcount() --  return the number of nodes in the given tree
 */
PUBLIC long avlcount(tree) AVL_TREE tree;
{
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  return avl_desc->count;
} /* avlcount */

/*
 * avlins() -- insert the given item into the tree structure
 */
PUBLIC void* avlins(data, tree) void* data;
AVL_TREE tree;
{
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  avl_insert(&data, (*(avl_desc->isize))(data), &(avl_desc->root),
             avl_desc->compar);
  if (data == NULL) {
    ++avl_desc->count;
  }

  return data;
} /* avlins */

/*
 * avldel() -- delete the given item from the given tree structure
 */
PUBLIC void* avldel(data, tree) void* data;
AVL_TREE tree;
{
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  avl_delete(&data, &(avl_desc->root), avl_desc->compar);
  if (data != NULL) {
    --avl_desc->count;
  }

  return data;
} /* avldel */

/*
 * avlfind() -- find the given item in the given tree structure
 *              and return its address (NULL if not found).
 */
PUBLIC void* avlfind(data, tree) void* data;
AVL_TREE tree;
{
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  return avl_find(data, avl_desc->root, avl_desc->compar);
} /* avlfind */

/*
 * avlfindex() -- find the given item in the given tree structure
 * 		using given compare function
 *              and return its address (NULL if not found).
 */
PUBLIC void* avlfindex(compar, data, tree) long (*compar)();
void* data;
AVL_TREE tree;
{
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  return avl_find(data, avl_desc->root, compar);
} /* avlfinex */

/*
 * avldelmin() -- delete the minimal item from the given tree structure
 */
PUBLIC void* avldelmin(tree) AVL_TREE tree;
{
  void* data;
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  avl_delete(&data, &(avl_desc->root), avl_min);
  if (data != NULL) {
    --avl_desc->count;
  }

  return data;
} /* avldelmin */

/*
 * avlfindmin() -- find the minimal item in the given tree structure
 *              and return its address (NULL if not found).
 */
PUBLIC void* avlfindmin(tree) AVL_TREE tree;
{
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  return avl_find(NULL, avl_desc->root, avl_min);
} /* avlfindmin */

/*
 * avldelmax() -- delete the maximal item from the given tree structure
 */
PUBLIC void* avldelmax(tree) AVL_TREE tree;
{
  void* data;
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  avl_delete(&data, &(avl_desc->root), avl_max);
  if (data != NULL) {
    --avl_desc->count;
  }

  return data;
} /* avldelmax */

/*
 * avlfindmax() -- find the maximal item in the given tree structure
 *              and return its address (NULL if not found).
 */
PUBLIC void* avlfindmax(tree) AVL_TREE tree;
{
  AVLdescriptor* avl_desc;

  avl_desc = (AVLdescriptor*)tree;
  return avl_find(NULL, avl_desc->root, avl_max);
} /* avlfindmax */
