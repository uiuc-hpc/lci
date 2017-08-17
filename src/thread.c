#include "thread.h"

void lc_sync_init(lc_wait_fp w, lc_signal_fp s, lc_yield_fp y)
{
  g_sync.wait = w;
  g_sync.signal = s;
  g_sync.yield = y;
}
