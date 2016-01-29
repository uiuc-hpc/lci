#ifndef CONFIG_H_
#define CONFIG_H_

static const int MAX_CONCURRENCY = 128;
static const int PACKET_SIZE = (16 * 1024);
static const int SHORT = (PACKET_SIZE - 64);
static const int INLINE = 512;
static const int NSBUF = MAX_CONCURRENCY;
static const int NPREPOST = 16;
static const size_t HEAP_SIZE = 1 * 1024 * 1024 * 1024;

/** hash_table */
//#define USE_COCK
//#define USE_LF
#define USE_ARRAY

/** profiling */
// #define USE_TIMING

// #define USE_PAPI
#endif
