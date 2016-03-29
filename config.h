#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdlib.h>

static const int MAX_SEND = 32;  // maximum concurrent send.
static const int MAX_RECV = 32;  // maximum concurrent recv.
static const int MAX_CONCURRENCY = MAX_SEND + MAX_RECV;
static const int PACKET_SIZE = (16 * 1024); // transfer unit size.
static const int SHORT_MSG_SIZE = (PACKET_SIZE - 16); // short message size.
static const int SERVER_COPY_SIZE = SHORT_MSG_SIZE; // threshold to which server poll and copy.
static const int RNDZ_MSG_SIZE = 48;

static const size_t HEAP_SIZE =
    (size_t)2 * 1024 * 1024 * 1024;  // total pinned heap size.

/** hash_table */
//#define USE_COCK
//#define USE_LF
#define USE_ARRAY

/** profiling */
// #define USE_TIMING

//#define USE_PAPI
//#define USE_AFFI

#ifdef USE_LF
#include "lf_hashtbl.h"
#endif

#ifdef USE_COCK
#include "cock_hashtbl.h"
#endif

#ifdef USE_ARRAY
#include "arr_hashtbl.h"
#endif

#endif
