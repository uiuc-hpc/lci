/** <plaintext>
*
* avl_typs.h -- declaration of private types used for avl trees
*
* Created 03/01/89 by Brad Appleton
*
* ^{Mods:* }
*
* Fri Jul 14 13:55:58 1989, Rev 1.0, brad(0165)
*
**/

/* definition of a NULL action and a NULL tree */
#define NULL_ACTION ((void(*)()) NULL)
#define NULL_TREE ((AVLtree) NULL)

/* MIN and MAX macros (used for rebalancing) */
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* Directional Definitions */
typedef short DIRECTION;
#define LEFT 0
#define RIGHT 1
#define OPPOSITE(x) (1 - (x))

/* return codes used by avl_insert(), avl_delete(), and balance() */
#define HEIGHT_UNCHANGED 0
#define HEIGHT_CHANGED 1

/* Balance Definitions */
#define LEFT_HEAVY -1
#define BALANCED 0
#define RIGHT_HEAVY 1
#define LEFT_IMBALANCE(nd) ((nd)->bal < LEFT_HEAVY)
#define RIGHT_IMBALANCE(nd) ((nd)->bal > RIGHT_HEAVY)

/* structure for a node in an AVL tree */
typedef struct avl_node {
    void* data;                     /* pointer to data */
    short bal;                      /* balance factor */
    struct avl_node* subtree[2];    /* LEFT and RIGHT subtrees */
#if !defined(DISABLE_PTMALLOC)
    /* A next field to thread AVLnodes in case of free with PTMALLOC. */
    void* next;
#endif /* !defined(DISABLE_PTMALLOC) */
} AVLnode, *AVLtree;

/* structure which holds information about an AVL tree */
typedef struct avl_descriptor {
    AVLtree root;                /* pointer to the root node of the tree */
    long (*compar)();            /* function used to compare keys */
    unsigned long (*isize)();    /* function to return the size of an item */
    long count;                  /* number of nodes in the tree */
} AVLdescriptor;
