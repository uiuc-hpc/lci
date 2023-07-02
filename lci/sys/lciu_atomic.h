#ifndef LCI_LCIU_ATOMIC_H
#define LCI_LCIU_ATOMIC_H

#include <stdatomic.h>

// Provide one layer of indirection for the ease of debugging
// e.g. we can easily set all memory order to memory_order_seq_cst
#define LCIU_memory_order_seq_cst memory_order_seq_cst
#define LCIU_memory_order_acquire memory_order_acquire
#define LCIU_memory_order_release memory_order_release
#define LCIU_memory_order_relaxed memory_order_relaxed

#endif  // LCI_LCIU_ATOMIC_H
