#ifndef ULT_H_
#define ULT_H_

#include <stdint.h>

struct mv_sync;
typedef struct mv_sync mv_sync;

struct mv_sync* mv_get_sync();
struct mv_sync* mv_get_counter(int count);
void thread_wait(mv_sync* sync);
void thread_signal(mv_sync* sync);

#endif
