#include "runtime/lcii.h"

#ifdef LCI_USE_PAPI
#include "papi.h"

#define PAPI_SAFECALL(x)                                                 \
  {                                                                      \
    int err = (x);                                                       \
    if (err != PAPI_OK) {                                                \
      LCM_Log(LCM_LOG_WARN, "papi", "err %d: %s\n",             \
              err, PAPI_strerror(err));                                  \
      return;                                                            \
    }                                                                    \
  }                                                                      \
  while (0)                                                              \
    ;

int event_set = PAPI_NULL;
bool enabled;

void LCII_papi_init() {
  enabled = false;
  char* event_str = getenv("LCI_PAPI_EVENTS");
  if (event_str == NULL)
    return;

  int retval;
  retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    LCM_Log(LCM_LOG_WARN, "papi", "PAPI library init error!\n");
    return;
  }
  PAPI_SAFECALL(PAPI_create_eventset(&event_set));

  const char delimiter = ';';
  char *p = event_str;
  char *q = event_str;
  char event_code_str[PAPI_MAX_STR_LEN];
  while (true) {
    if (*p != delimiter && *p != '\0') {
      ++p;
      continue;
    }
    if (p == q) {
      // empty string
      if (*p == '\0')
        break;
      q = ++p;
      continue;
    }
    LCM_Assert(p - q < sizeof(event_code_str), "Unexpected string length %lu!\n", p - q);
    // the string is between q and p
    memset(event_code_str, 0, sizeof(event_code_str));
    memcpy(event_code_str, q, p - q);
    // Translate the string to event code
    int ret = PAPI_add_named_event(event_set, event_code_str);
//    int event_code;
//    int ret = PAPI_event_name_to_code(event_code_str, &event_code);
    if (ret == PAPI_OK) {
//      PAPI_SAFECALL(PAPI_add_event(event_set, event_code));
//      LCM_Log(LCM_LOG_INFO, "papi", "Add event %s(%x)\n", event_code_str, event_code);
      LCM_Log(LCM_LOG_INFO, "papi", "Add event %s\n", event_code_str);
    } else {
      LCM_Log(LCM_LOG_WARN, "papi", "Cannot figure out event \"%s\" (%s)\n", event_code_str, PAPI_strerror(ret));
    }
    // move to the next character
    if (*p == '\0')
      break;
    else
      q = ++p;
  }

  if (PAPI_num_events(event_set) > 0) {
    PAPI_SAFECALL(PAPI_start(event_set));
    enabled = true;
  } else {
    LCM_Log(LCM_LOG_WARN, "papi", "No valid event detected!\n");
  }
}

void LCII_papi_fina() {
  if (!enabled)
    return;

  int num_events = PAPI_num_events(event_set);
  int *event_codes = LCIU_malloc(num_events * sizeof(int));
  long long *event_counters = LCIU_malloc(num_events * sizeof(long long));

  PAPI_SAFECALL(PAPI_stop(event_set, event_counters));

  int number = num_events;
  PAPI_SAFECALL(PAPI_list_events(event_set, event_codes, &number));
  LCM_Assert(num_events == number, "Unexpected event count!\n");

  char event_code_str[PAPI_MAX_STR_LEN];
  static char buf[1024];
  size_t consumed = 0;
  consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                       "rank,papi_event,count\n");
  LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  for (int i = 0; i < num_events; ++i) {
    PAPI_SAFECALL(PAPI_event_code_to_name(event_codes[i], event_code_str));
    consumed += snprintf(buf + consumed, sizeof(buf) - consumed,
                         "%d,%s,%lld\n",
                         LCI_RANK, event_code_str, event_counters[i]);
    LCM_Assert(sizeof(buf) > consumed, "buffer overflowed!\n");
  }
  LCM_Log(LCM_LOG_TRACE, "papi", "\nPAPI counters:\n%s", buf);

  LCIU_free(event_codes);
  LCIU_free(event_counters);
}
#else
void LCII_papi_init() {}
void LCII_papi_fina() {}
#endif