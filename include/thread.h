#ifndef LC_THREAD_H_
#define LC_THREAD_H_

#include "affinity.h"
#include "macro.h"
#include "lock.h"

#include <sched.h>
#include <stdint.h>

extern int lcg_current_id;
extern __thread int lcg_core_id;

#endif
