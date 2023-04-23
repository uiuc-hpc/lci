#include "runtime/lcii.h"

// needed by the monitor threads
static pthread_t LCII_monitor_thread;
static atomic_bool LCII_monitor_thread_run;
static bool LCI_ENABLE_MONITOR_THREAD = false;
static int LCI_MONITOR_THREAD_INTERVAL;

struct timespec LCIU_timespec_diff(struct timespec new, struct timespec old)
{
  struct timespec diff;
  if (new.tv_nsec >= old.tv_nsec) {
    diff.tv_sec = new.tv_sec - old.tv_sec;
    diff.tv_nsec = new.tv_nsec - old.tv_nsec;
  } else {
    diff.tv_sec = new.tv_sec - old.tv_sec - 1;
    diff.tv_nsec = new.tv_nsec - old.tv_nsec + 1000000000;
  }
  diff.tv_sec += diff.tv_nsec / 1000000000;
  diff.tv_nsec %= 1000000000;
  return diff;
}

void* LCII_monitor_thread_fn(void* vargp)
{
  struct timespec start_time, time_now;
  LCII_pcounters_per_thread_t pcounter_now, pcounter_old, pcounter_diff;
  memset(&pcounter_old, 0, sizeof(pcounter_old));
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  LCM_Log(LCM_LOG_INFO, "monitor", "Start the monitor thread at %lu.%lu s\n",
          start_time.tv_sec, start_time.tv_nsec);
  LCM_Log_flush();
  while (atomic_load_explicit(&LCII_monitor_thread_run,
                              LCIU_memory_order_acquire)) {
    sleep(LCI_MONITOR_THREAD_INTERVAL);
    pcounter_now = LCII_pcounters_accumulate();
    pcounter_diff = LCII_pcounters_diff(pcounter_now, pcounter_old);
    pcounter_old = pcounter_now;
    clock_gettime(CLOCK_MONOTONIC, &time_now);
    struct timespec time_diff = LCIU_timespec_diff(time_now, start_time);
    char* p = NULL;
#ifdef LCI_USE_PERFORMANCE_COUNTER
    p = LCII_pcounters_to_string(pcounter_diff);
#else
    p = "No performance counter available.\n";
#endif
    LCM_Log(LCM_LOG_INFO, "monitor", "Time %lu.%lu s\n%s", time_diff.tv_sec,
            time_diff.tv_nsec, p);
    LCM_Log_flush();
  }
  clock_gettime(CLOCK_MONOTONIC, &time_now);
  struct timespec time_diff = LCIU_timespec_diff(time_now, start_time);
  LCM_Log(LCM_LOG_INFO, "monitor", "Finish the monitor thread at %lu.%lu s\n",
          time_diff.tv_sec, time_diff.tv_nsec);
  LCM_Log_flush();
  return NULL;
}

void LCII_monitor_thread_init()
{
  LCI_ENABLE_MONITOR_THREAD =
      LCIU_getenv_or("LCI_ENABLE_MONITOR_THREAD", false);
  if (LCI_ENABLE_MONITOR_THREAD) {
    LCI_MONITOR_THREAD_INTERVAL =
        LCIU_getenv_or("LCI_MONITOR_THREAD_INTERVAL", 60);
    atomic_init(&LCII_monitor_thread_run, true);
    pthread_create(&LCII_monitor_thread, NULL, LCII_monitor_thread_fn, NULL);
  }
}

void LCII_monitor_thread_fina()
{
  if (LCI_ENABLE_MONITOR_THREAD) {
    atomic_store_explicit(&LCII_monitor_thread_run, false,
                          LCIU_memory_order_release);
    pthread_join(LCII_monitor_thread, NULL);
  }
}