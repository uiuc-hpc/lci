#include "lcii.h"

pthread_t thread_id;
volatile bool thread_run;
volatile int LCII_hang_detector_signal;
bool LCII_enable_hang_detector = false;
int LCII_hang_detector_timeout;

void timeout_callback() {
  LCM_Warn("Hang timeout callback is invoked!\n");
  LCM_Log_flush();
  LCI_barrier();
}

void *hang_detector_fn(void *vargp)
{
  while (thread_run) {
    if (LCII_hang_detector_signal == 1) {
      LCII_hang_detector_signal = 0;
    } else {
      timeout_callback();
      abort();
    }
    sleep(LCII_hang_detector_timeout);
  }
  return NULL;
}

void LCII_hang_detector_init() {
  LCII_enable_hang_detector = getenv_or("LCI_ENABLE_HANG_DETECTOR", false);
  if (LCII_enable_hang_detector) {
    LCII_hang_detector_timeout = getenv_or("LCII_HANG_DETECTOR_TIMEOUT", 60);
    thread_run = true;
    LCII_hang_detector_signal = 1;
    pthread_create(&thread_id, NULL, hang_detector_fn, NULL);
  }
}

void LCII_hang_detector_fina() {
  if (LCII_enable_hang_detector) {
    thread_run = false;
    pthread_join(thread_id, NULL);
  }
}