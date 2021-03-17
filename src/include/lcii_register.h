#ifndef LCI_LCII_REGISTER_H
#define LCI_LCII_REGISTER_H

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "lcii.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Used to map address to key with less bits.
 */

#define LCII_REG_NTRY 3
#define LCII_REG_EMPTY 0

typedef uint64_t LCII_reg_key_t;
typedef uintptr_t LCII_reg_value_t;

struct LCII_reg_entry_t {
  LCII_reg_value_t val;   // 8 bytes
  char padding[64-sizeof(LCII_reg_value_t)];
};

struct LCII_register_t {
  volatile struct LCII_reg_entry_t* ptr;
  int nbits;
};
typedef struct LCII_register_t LCII_register_t;

static LCI_error_t LCII_register_init(LCII_register_t* reg, int nbits);
static LCI_error_t LCII_register_fini(LCII_register_t* reg);

static inline LCI_error_t LCII_register_put(LCII_register_t reg,
                                            LCII_reg_value_t value,
                                            LCII_reg_key_t* key);
static inline LCII_reg_value_t LCII_register_get(LCII_register_t reg,
                                                 LCII_reg_key_t key);
static inline LCII_reg_value_t LCII_register_remove(LCII_register_t reg,
                                                    LCII_reg_key_t key);

#ifdef __cplusplus
}
#endif

static LCI_error_t LCII_register_init(LCII_register_t* reg, int nbits)
{
  assert(sizeof(struct LCII_reg_entry_t) == 64);
  size_t cap = 1 << nbits;
  reg->ptr = LCIU_malloc(cap * sizeof(struct LCII_reg_entry_t));
  reg->nbits = nbits;

  // Initialize all with EMPTY and clear lock.
  for (size_t i = 0; i < cap; i++) {
    // First are control.
    reg->ptr[i].val = LCII_REG_EMPTY;
  }
  return LCI_OK;
}

static LCI_error_t LCII_register_fini(LCII_register_t* reg)
{
  LCIU_free((void*) reg->ptr);
  reg->ptr = NULL;
  reg->nbits = 0;
  return LCI_OK;
}

static inline LCI_error_t LCII_register_put(LCII_register_t reg,
                                            LCII_reg_value_t value,
                                            LCII_reg_key_t* key_ptr)
{
  LCII_reg_key_t key = (value >> 6) & (1 << reg.nbits - 1);
  for (int i = 1; i <= LCII_REG_NTRY; ++i) {
    if (reg.ptr[key].val == LCII_REG_EMPTY) {
      if (__sync_bool_compare_and_swap(&(reg.ptr[key].val), LCII_REG_EMPTY,
                                       value)) {
        *key_ptr = key;
        return LCI_OK;
      }
    }
    // quadratic probe
    key += i * i;
  }
  return LCI_ERR_RETRY;
}

static inline LCII_reg_value_t LCII_register_get(LCII_register_t reg,
                                                 LCII_reg_key_t key)
{
  return reg.ptr[key].val;
}

static inline LCII_reg_value_t LCII_register_remove(LCII_register_t reg,
                                                    LCII_reg_key_t key)
{
  LCII_reg_value_t old;
  LCII_reg_value_t val = reg.ptr[key].val;
  while (true) {
    old = __sync_val_compare_and_swap(&(reg.ptr[key].val), val,
                                      LCII_REG_EMPTY);
    if (old == val) {
      // succeeded!
      break;
    } else {
      // failed! Try again!
      val = old;
    }
  }
  return val;
}

#endif // LCI_LCII_REGISTER_H
