#include "lcii.h"

// needed by the flush threads
static pthread_t LCII_monitor_thread;
static volatile bool LCII_monitor_thread_run;
static bool LCI_ENABLE_MONITOR_THREAD = false;
static int LCI_MONITOR_THREAD_INTERVAL;

struct timespec LCIU_timespec_diff(struct timespec new, struct timespec old) {
  struct timespec diff;
  if (new.tv_nsec >= old.tv_nsec) {
    diff.tv_sec = new.tv_sec - old.tv_sec;
    diff.tv_nsec = new.tv_nsec - old.tv_nsec;
  } else {
    diff.tv_sec = new.tv_sec - old.tv_sec - 1;
    diff.tv_nsec = new.tv_nsec - old.tv_nsec + 1000000000;
  }
  return diff;
}

void *LCII_monitor_thread_fn(void *vargp)
{
  struct timespec start_time, now;
  LCII_pcounters_per_thread_t pcounter, old;
  memset(&old, 0, sizeof(old));
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  LCM_Log(LCM_LOG_INFO, "monitor", "Start the monitor thread at %lu.%lu s\n",
          start_time.tv_sec, start_time.tv_nsec);
  LCM_Log_flush();
  while (LCII_monitor_thread_run) {
    sleep(LCI_MONITOR_THREAD_INTERVAL);
    pcounter = LCII_pcounters_accumulate();
    clock_gettime(CLOCK_MONOTONIC, &now);
    struct timespec diff = LCIU_timespec_diff(now, start_time);
    LCM_Log(LCM_LOG_INFO, "monitor", "Time %lu.%lu s: msgs_send %ld "
            "bytes_send %ld msgs_recv %ld bytes_recv %ld\n",
            diff.tv_sec, diff.tv_nsec,
            pcounter.msgs_send - old.msgs_send,
            pcounter.bytes_send - old.bytes_send,
            pcounter.msgs_recv - old.msgs_recv,
            pcounter.bytes_recv - old.bytes_recv);
    old = pcounter;
    LCM_Log_flush();
  }
  clock_gettime(CLOCK_MONOTONIC, &now);
  struct timespec diff = LCIU_timespec_diff(now, start_time);
  LCM_Log(LCM_LOG_INFO, "monitor", "Finish the monitor thread at %lu.%lu s: "
          "msgs_send %ld bytes_send %ld msgs_recv %ld bytes_recv %ld in total\n",
          diff.tv_sec, diff.tv_nsec,
          pcounter.msgs_send, pcounter.bytes_send,
          pcounter.msgs_recv, pcounter.bytes_recv);
  LCM_Log_flush();
  return NULL;
}

void LCII_monitor_thread_init() {
  // flush threads
  LCI_ENABLE_MONITOR_THREAD = getenv_or("LCI_ENABLE_MONITOR_THREAD", false);
  if (LCI_ENABLE_MONITOR_THREAD) {
    LCI_MONITOR_THREAD_INTERVAL = getenv_or("LCI_MONITOR_THREAD_INTERVAL", 60);
    LCII_monitor_thread_run = true;
    pthread_create(&LCII_monitor_thread, NULL, LCII_monitor_thread_fn, NULL);
  }
}

void LCII_monitor_thread_fina() {
  if (LCI_ENABLE_MONITOR_THREAD) {
    LCII_monitor_thread_run = false;
    pthread_join(LCII_monitor_thread, NULL);
  }
}