#include <assert.h>
#include <inttypes.h> /* for PRI___ macros (printf) */
#include <stdio.h>
#include <stdlib.h> /* for malloc/free/etc */
#include <unistd.h> /* for getpagesize() */

/*
 * The hash table in this file is based on the work by Ori Shalev and Nir Shavit
 * Much of the code is similar to the example code they published in their
 * paper "Split-Ordered Lists: Lock-Free Extensible Hash Tables", but has been
 * modified to support additional semantics.
 *
 * NOTE: I did not implement the Dynamic-Sized Array extension, because I don't
 * expect to support large numbers of entries in the table at the same time,
 * and I'm worried about state creep (this algorithm cannot compact well).
 */

#define MAX_LOAD 4
#define USE_HASHWORD 0

/* external types */
typedef const void* qt_lf_key_t;
typedef struct qt_hash_s* qt_hash;
typedef void (*qt_hash_callback_fn)(const qt_lf_key_t, void*, void*);
typedef void (*qt_hash_deallocator_fn)(void*);

/* internal types */
typedef uint64_t lf_key_t;
typedef uint64_t so_lf_key_t;
typedef uintptr_t marked_ptr_t;

#define MARK_OF(x) ((x)&1)
#define PTR_MASK(x) ((x) & ~(marked_ptr_t)1)
#define PTR_OF(x) ((hash_entry*)PTR_MASK(x))
#define CONSTRUCT(mark, ptr) (PTR_MASK((uintptr_t)ptr) | (mark))
#define UNINITIALIZED ((marked_ptr_t)0)

/* These are GCC builtins; other compilers may require inline assembly */
#define CAS(ADDR, OLDV, NEWV) \
  __sync_val_compare_and_swap((ADDR), (OLDV), (NEWV))
#define INCR(ADDR, INCVAL) __sync_fetch_and_add((ADDR), (INCVAL))

size_t hard_max_buckets = 0;

typedef struct hash_entry_s {
  so_lf_key_t key;
  void* value;
  marked_ptr_t next;
} hash_entry;

struct qt_hash_s {
  marked_ptr_t* B;  // Buckets
  volatile size_t count;
  volatile size_t size;
};

#if 0
#define POOL_SIZE 4096
static hash_entry POOL[POOL_SIZE];
static volatile int POOL_index = 0;
#endif

/* prototypes */
static void* qt_lf_list_find(marked_ptr_t* head, so_lf_key_t key,
                             marked_ptr_t** prev, marked_ptr_t* cur,
                             marked_ptr_t* next);
static void initialize_bucket(qt_hash h, size_t bucket);

#define MSB (((uint64_t)1) << 63)
#define REVERSE_BYTE(x)                                        \
  ((so_lf_key_t)((((((uint32_t)(x)) * 0x0802LU & 0x22110LU) |  \
                   (((uint32_t)(x)) * 0x8020LU & 0x88440LU)) * \
                      0x10101LU >>                             \
                  16) &                                        \
                 0xff))
#if 0
/* 32-bit reverse */
#define REVERSE(x)                                             \
  (REVERSE_BYTE((((so_lf_key_t)(x))) & 0xff) << 24) |          \
      (REVERSE_BYTE((((so_lf_key_t)(x)) >> 8) & 0xff) << 16) | \
      (REVERSE_BYTE((((so_lf_key_t)(x)) >> 16) & 0xff) << 8) | \
      (REVERSE_BYTE((((so_lf_key_t)(x)) >> 24) & 0xff))
#else
#define REVERSE(x)                                           \
  ((REVERSE_BYTE((((so_lf_key_t)(x))) & 0xff) << 56) |       \
   (REVERSE_BYTE((((so_lf_key_t)(x)) >> 8) & 0xff) << 48) |  \
   (REVERSE_BYTE((((so_lf_key_t)(x)) >> 16) & 0xff) << 40) | \
   (REVERSE_BYTE((((so_lf_key_t)(x)) >> 24) & 0xff) << 32) | \
   (REVERSE_BYTE((((so_lf_key_t)(x)) >> 32) & 0xff) << 24) | \
   (REVERSE_BYTE((((so_lf_key_t)(x)) >> 40) & 0xff) << 16) | \
   (REVERSE_BYTE((((so_lf_key_t)(x)) >> 48) & 0xff) << 8) |  \
   (REVERSE_BYTE((((so_lf_key_t)(x)) >> 56) & 0xff) << 0))
#endif /* if 0 */

#ifdef USE_HASHWORD
#define HASH_KEY(key) key = qt_hashword(key)
/* this function based on http://burtleburtle.net/bob/hash/evahash.html */
#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
static uint64_t qt_hashword(uint64_t key)
{ /*{{{*/
  uint32_t a, b, c;

  const union {
    uint64_t key;
    uint8_t b[sizeof(uint64_t)];
  } k = {key};

  a = b = c =
      0x32533d0c + sizeof(uint64_t);  // an arbitrary value, randomly selected
  c += 47;

  b += k.b[7] << 24;
  b += k.b[6] << 16;
  b += k.b[5] << 8;
  b += k.b[4];
  a += k.b[3] << 24;
  a += k.b[2] << 16;
  a += k.b[1] << 8;
  a += k.b[0];

  c ^= b;
  c -= rot(b, 14);
  a ^= c;
  a -= rot(c, 11);
  b ^= a;
  b -= rot(a, 25);
  c ^= b;
  c -= rot(b, 16);
  a ^= c;
  a -= rot(c, 4);
  b ^= a;
  b -= rot(a, 14);
  c ^= b;
  c -= rot(b, 24);
  return ((uint64_t)c + (((uint64_t)b) << 32)) & (~MSB);
} /*}}}*/

#else /* ifdef USE_HASHWORD */
#define HASH_KEY(key)
#endif /* ifdef USE_HASHWORD */

static int qt_lf_list_insert(marked_ptr_t* head, hash_entry* node,
                             marked_ptr_t* ocur)
{
  so_lf_key_t key = node->key;

  while (1) {
    marked_ptr_t* lprev;
    marked_ptr_t cur;

    if (qt_lf_list_find(head, key, &lprev, &cur, NULL) !=
        NULL) {  // needs to set cur/prev
      if (ocur) {
        *ocur = cur;
      }
      return 0;
    }
    node->next = CONSTRUCT(0, cur);
    if (CAS(lprev, node->next, CONSTRUCT(0, node)) == CONSTRUCT(0, cur)) {
      if (ocur) {
        *ocur = cur;
      }
      return 1;
    }
  }
}

static int qt_lf_list_delete(marked_ptr_t* head, so_lf_key_t key)
{
  while (1) {
    marked_ptr_t* lprev;
    marked_ptr_t lcur;
    marked_ptr_t lnext;

    if (qt_lf_list_find(head, key, &lprev, &lcur, &lnext) == NULL) {
      return 0;
    }
    if (CAS(&PTR_OF(lcur)->next, CONSTRUCT(0, lnext), CONSTRUCT(1, lnext)) !=
        CONSTRUCT(0, lnext)) {
      continue;
    }
    if (CAS(lprev, CONSTRUCT(0, lcur), CONSTRUCT(0, lnext)) ==
        CONSTRUCT(0, lcur)) {
      free(PTR_OF(lcur));
    } else {
      qt_lf_list_find(head, key, NULL, NULL,
                      NULL);  // needs to set cur/prev/next
    }
    return 1;
  }
}

static void* qt_lf_list_find(marked_ptr_t* head, so_lf_key_t key,
                             marked_ptr_t** oprev, marked_ptr_t* ocur,
                             marked_ptr_t* onext)
{
  so_lf_key_t ckey;
  void* cval;
  marked_ptr_t* prev = NULL;
  marked_ptr_t cur = UNINITIALIZED;
  marked_ptr_t next = UNINITIALIZED;

  while (1) {
    prev = head;
    cur = *prev;
    while (1) {
      if (PTR_OF(cur) == NULL) {
        if (oprev) {
          *oprev = prev;
        }
        if (ocur) {
          *ocur = cur;
        }
        if (onext) {
          *onext = next;
        }
        return 0;
      }
      next = PTR_OF(cur)->next;
      ckey = PTR_OF(cur)->key;
      cval = PTR_OF(cur)->value;
      if (*prev != CONSTRUCT(0, cur)) {
        break;  // this means someone mucked with the list; start over
      }
      if (!MARK_OF(next)) {  // if next pointer is not marked
        if (ckey >= key) {   // if current key > key, the key isn't in the list;
                             // if current key == key, the key IS in the list
          if (oprev) {
            *oprev = prev;
          }
          if (ocur) {
            *ocur = cur;
          }
          if (onext) {
            *onext = next;
          }
          return (ckey == key) ? cval : NULL;
        }
        // but if current key < key, the we don't know yet, keep looking
        prev = &(PTR_OF(cur)->next);
      } else {
        if (CAS(prev, CONSTRUCT(0, cur), CONSTRUCT(0, next)) ==
            CONSTRUCT(0, cur)) {
          free(PTR_OF(cur));
        } else {
          break;
        }
      }
      cur = next;
    }
  }
}

static inline so_lf_key_t so_regularkey(const key_t key)
{
  return REVERSE(key | MSB);
}

static inline so_lf_key_t so_dummykey(const key_t key) { return REVERSE(key); }
void* qt_hash_put(qt_hash h, qt_lf_key_t key, void* value)
{
  // int index = __sync_fetch_and_add(&POOL_index, 1) & (POOL_SIZE - 1);
  // hash_entry *node = &POOL[index];
  // memset(node, 0, sizeof(hash_entry));
  hash_entry* node = (hash_entry*)malloc(sizeof(hash_entry));

  size_t bucket;
  uint64_t lkey = (uint64_t)(uintptr_t)key;

  HASH_KEY(lkey);
  bucket = lkey % h->size;

  assert(node);
  assert((lkey & MSB) == 0);
  node->key = so_regularkey(lkey);
  node->value = value;
  node->next = UNINITIALIZED;

  if (h->B[bucket] == UNINITIALIZED) {
    initialize_bucket(h, bucket);
  }
  uintptr_t rs;
  if (!qt_lf_list_insert(&(h->B[bucket]), node, &rs)) {
    free(node);
    return PTR_OF(rs)->value;
  }
  size_t csize = h->size;
  if (INCR(&h->count, 1) / csize > MAX_LOAD) {
    if (2 * csize <= hard_max_buckets) {  // this caps the size of the hash
      CAS(&h->size, csize, 2 * csize);
    }
  }
  return value;
}

void* qt_hash_get(qt_hash h, const qt_lf_key_t key)
{
  size_t bucket;
  uint64_t lkey = (uint64_t)(uintptr_t)key;

  HASH_KEY(lkey);
  bucket = lkey % h->size;

  if (h->B[bucket] == UNINITIALIZED) {
    // You'd think returning NULL at this point would be a good idea; but
    // if we do that, we risk losing key/value pairs (incorrectly reporting
    // them as absent) when the hash table resizes
    initialize_bucket(h, bucket);
  }
  return qt_lf_list_find(&(h->B[bucket]), so_regularkey(lkey), NULL, NULL,
                         NULL);
}

int qt_hash_remove(qt_hash h, const qt_lf_key_t key)
{
  size_t bucket;
  uint64_t lkey = (uint64_t)(uintptr_t)key;

  HASH_KEY(lkey);
  bucket = lkey % h->size;

  if (h->B[bucket] == UNINITIALIZED) {
    initialize_bucket(h, bucket);
  }
  if (!qt_lf_list_delete(&(h->B[bucket]), so_regularkey(lkey))) {
    return 0;
  }
  INCR(&h->count, -1);
  return 1;
}

static inline size_t GET_PARENT(uint64_t bucket)
{
  uint64_t t = bucket;

  t |= t >> 1;
  t |= t >> 2;
  t |= t >> 4;
  t |= t >> 8;
  t |= t >> 16;
  t |= t >> 32;  // creates a mask
  return bucket & (t >> 1);
}

static void initialize_bucket(qt_hash h, size_t bucket)
{
  size_t parent = GET_PARENT(bucket);
  marked_ptr_t cur;

  if (h->B[parent] == UNINITIALIZED) {
    initialize_bucket(h, parent);
  }
  // int index = __sync_fetch_and_add(&POOL_index, 1) & (POOL_SIZE - 1);
  // hash_entry *dummy = &POOL[index];
  // memset(dummy, 0, sizeof(hash_entry));
  hash_entry* dummy = (hash_entry*)malloc(sizeof(hash_entry));

  dummy->key = so_dummykey(bucket);
  dummy->value = NULL;
  dummy->next = UNINITIALIZED;
  if (!qt_lf_list_insert(&(h->B[parent]), dummy, &cur)) {
    free(dummy);
    dummy = PTR_OF(cur);
    while (h->B[bucket] != CONSTRUCT(0, dummy))
      ;
  } else {
    h->B[bucket] = CONSTRUCT(0, dummy);
  }
}

qt_hash qt_hash_create(int needSync)
{
  qt_hash tmp = (qt_hash)malloc(sizeof(struct qt_hash_s));

  assert(tmp);
  if (hard_max_buckets == 0) {
    hard_max_buckets = getpagesize() / sizeof(marked_ptr_t);
  }
  tmp->B = (marked_ptr_t*)calloc(hard_max_buckets, sizeof(marked_ptr_t));
  assert(tmp->B);
  tmp->size = 2;
  tmp->count = 0;
  {
    hash_entry* dummy = (hash_entry*)calloc(
        1, sizeof(hash_entry));  // XXX: should pull out of a memory pool
    assert(dummy);
    tmp->B[0] = CONSTRUCT(0, dummy);
  }
  return tmp;
}

void qt_hash_destroy(qt_hash h)
{
  marked_ptr_t cursor;

  assert(h);
  assert(h->B);
  cursor = h->B[0];
  while (PTR_OF(cursor) != NULL) {
    marked_ptr_t tmp = cursor;
    assert(MARK_OF(tmp) == 0);
    cursor = PTR_OF(cursor)->next;
    free(PTR_OF(tmp));
  }
  free(h->B);
  free(h);
}

void qt_hash_destroy_deallocate(qt_hash h, qt_hash_deallocator_fn f)
{
  marked_ptr_t cursor;

  assert(h);
  assert(h->B);
  cursor = h->B[0];
  while (PTR_OF(cursor) != NULL) {
    marked_ptr_t tmp = cursor;
    assert(MARK_OF(tmp) == 0);
    cursor = PTR_OF(cursor)->next;
    f(PTR_OF(cursor));
    free(PTR_OF(tmp));
  }
  free(h->B);
  free(h);
}

size_t qt_hash_count(qt_hash h)
{
  assert(h);
  return h->size;
}

void qt_hash_callback(qt_hash h, qt_hash_callback_fn f, void* arg)
{
  marked_ptr_t cursor;

  assert(h);
  assert(h->B);
  cursor = h->B[0];
  while (PTR_OF(cursor) != NULL) {
    marked_ptr_t tmp = cursor;
    so_lf_key_t key = PTR_OF(cursor)->key;
    assert(MARK_OF(tmp) == 0);
    // printf("%p = ", PTR_OF(cursor));
    if (key & 1) {
      f((qt_lf_key_t)(uintptr_t)REVERSE(key ^ 1), PTR_OF(cursor)->value, arg);
    } else {
      // f((qt_lf_key_t)REVERSE(key), PTR_OF(cursor)->value, (void *)1);
    }
    cursor = PTR_OF(cursor)->next;
  }
  /*
   * for (size_t i=0; i<h->size; ++i) {
   *  if (h->B[i] != UNINITIALIZED) {
   *      printf("%lu => %p\n", i, (void*)h->B[i]);
   *  }
   * }
   */
}

#if 0
void print_ent(const qt_lf_key_t k,
               void          *v,
               void          *a)
{
    if (a) {
        printf("\t{ %6"PRIuPTR",   ,\t%p }\n", (uintptr_t)k, /*(uintptr_t)v,*/ a);
    } else {
        printf("\t{ %6"PRIuPTR", %2"PRIuPTR",\t    }\n", (uintptr_t)k, (uintptr_t)v);
    }
}
int main()
{
    intptr_t i = 1;
    qt_hash  H = qt_hash_create(0);

    qt_hash_callback(H, print_ent, NULL);

    qt_hash_put(H, (void *)9, (void *)i++);
    printf("inserted 9\n");
    qt_hash_callback(H, print_ent, NULL);
    qt_hash_put(H, (void *)42, (void *)i++);
    printf("inserted 42\n");
    qt_hash_callback(H, print_ent, NULL);
    qt_hash_put(H, (void *)43, (void *)i++);
    printf("inserted 43\n");
    printf("H->count = %lu, H->size = %lu\n", H->count, H->size);

    qt_hash_callback(H, print_ent, NULL);

    printf("%s 9\n", qt_hash_get(H, (void *)9) ? "found" : "didn't find");
    printf("%s 10\n", qt_hash_get(H, (void *)10) ? "found" : "didn't find");
    printf("%s 42\n", qt_hash_get(H, (void *)42) ? "found" : "didn't find");
    printf("%s 43\n", qt_hash_get(H, (void *)43) ? "found" : "didn't find");

    qt_hash_remove(H, (void *)42);
    printf("deleted 42\n");

    qt_hash_callback(H, print_ent, NULL);

    printf("%s 9\n", qt_hash_get(H, (void *)9) ? "found" : "didn't find");
    printf("%s 10\n", qt_hash_get(H, (void *)10) ? "found" : "didn't find");
    printf("%s 42\n", qt_hash_get(H, (void *)42) ? "found" : "didn't find");
    printf("%s 43\n", qt_hash_get(H, (void *)43) ? "found" : "didn't find");

#if 1
    qt_hash_put(H, (void *)1024, (void *)i++);
    printf("inserted 1024 (%lu/%lu = %lu)\n", H->count, H->size, H->count / H->size);
    assert(qt_hash_get(H, (void *)1024));

    // qt_hash_callback(H, print_ent, NULL);

    qt_hash_put(H, (void *)96777, (void *)i++);
    printf("inserted 96777 (%lu/%lu = %lu)\n", H->count, H->size, H->count / H->size);
    assert(qt_hash_get(H, (void *)96777));

    // qt_hash_callback(H, print_ent, NULL);

    qt_hash_put(H, (void *)45260, (void *)i++);
    printf("inserted 45260 (%lu/%lu = %lu)\n", H->count, H->size, H->count / H->size);
    assert(qt_hash_get(H, (void *)45260));

    // qt_hash_callback(H, print_ent, NULL);

    qt_hash_put(H, (void *)61340, (void *)i++);
    printf("inserted 61340 (%lu/%lu = %lu)\n", H->count, H->size, H->count / H->size);
    assert(qt_hash_get(H, (void *)61340));

    // qt_hash_callback(H, print_ent, NULL);
#endif /* if 1 */

    for (intptr_t j = 11; j < 27; ++j) {
        qt_hash_put(H, (void *)j, (void *)i++);
        printf("inserted %li (%lu/%lu = %lu)\n", j, H->count, H->size, H->count / H->size);
        assert(qt_hash_get(H, (void *)j));
    }

    assert(qt_hash_get(H, (void *)9));
    assert(!qt_hash_get(H, (void *)10));
    assert(!qt_hash_get(H, (void *)42));
    assert(qt_hash_get(H, (void *)43));
    assert(qt_hash_get(H, (void *)1024));
    assert(qt_hash_get(H, (void *)96777));
    assert(qt_hash_get(H, (void *)45260));
    assert(qt_hash_get(H, (void *)61340));
    for (intptr_t j = 11; j < 27; ++j) {
        if (!qt_hash_get(H, (void *)j)) {
            printf("cannot find %li! It should have been there...\n", j);
            abort();
        }
    }
    printf("found everything I inserted!\n");

    qt_hash_destroy(H);
    return 0;
}
#endif

/* vim:set expandtab: */
