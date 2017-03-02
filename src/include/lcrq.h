#ifndef LCRQ_H_
#define LCRQ_H_

// Definition: RING_POW
// --------------------
// The LCRQ's ring size will be 2^{RING_POW}.
#ifndef RING_POW
#define RING_POW (17)
#endif
#define RING_SIZE (1ull << RING_POW)

typedef struct RingNode {
  volatile uint64_t val;
  volatile uint64_t idx;
  uint64_t pad[14];
} RingNode __attribute__((aligned(128)));

typedef struct RingQueue {
  volatile uint64_t head __attribute__((aligned(128)));
  volatile uint64_t tail __attribute__((aligned(128)));
  struct RingQueue* next __attribute__((aligned(128)));
  RingNode array[RING_SIZE];
} RingQueue __attribute__((aligned(128)));

typedef struct mpmc_queue {
  RingQueue* head;
  RingQueue* tail;
} lcrq_t;

void lcrq_init(lcrq_t* q);
void lcrq_destroy(lcrq_t* q);
void lcrq_enqueue(lcrq_t* q, void* item);
void* lcrq_dequeue(lcrq_t* q);

#endif
