#ifndef CONFIG_H_
#define CONFIG_H_

static const int MAX_CONCURRENCY = 128; // maximum concurrent send/recv allowed.
static const int PACKET_SIZE = (16 * 1024); // transfer unit size.
static const int SERVER_COPY_SIZE = 512; // threshold to which server poll and copy.
static const int SHORT = (PACKET_SIZE - 16); // short message size.
static const int NSBUF = MAX_CONCURRENCY;  // number of pinned buffer.
static const int NPREPOST = 16; // number of prepost message.
static const size_t HEAP_SIZE = (size_t) 2 * 1024 * 1024 * 1024; // total pinned heap size.

/** hash_table */
//#define USE_COCK
//#define USE_LF
#define USE_ARRAY

/** profiling */
// #define USE_TIMING

//#define USE_PAPI
//#define USE_AFFI

#endif
