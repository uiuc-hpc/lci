#ifndef LCM_LOG_H_
#define LCM_LOG_H_

#if defined(__cplusplus)
extern "C" {
#endif

#define LCM_API __attribute__((visibility("default")))

#define LCM_Assert(Expr, ...) \
        LCM_Assert_(#Expr, Expr, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LCM_Log(log_level, log_type, ...) \
        LCM_Log_(log_level, log_type, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LCM_Log_default(log_level, ...) \
        LCM_Log(log_level, "default", __VA_ARGS__)

#ifdef LCM_DEBUG
#define LCM_DBG_Assert(...) LCM_Assert(__VA_ARGS__)
#define LCM_DBG_Log(...) LCM_Log(__VA_ARGS__)
#define LCM_DBG_Log_default(...) LCM_Log_default(__VA_ARGS__)
#else
#define LCM_DBG_Assert(...)
#define LCM_DBG_Log(...)
#define LCM_DBG_Log_default(...)
#endif

enum LCM_log_level_t {
  LCM_LOG_NONE = 0,
  LCM_LOG_WARN,
  LCM_LOG_TRACE,
  LCM_LOG_INFO,
  LCM_LOG_DEBUG,
  LCM_LOG_MAX
};

LCM_API
extern void LCM_Init(int rank);

LCM_API
void LCM_Fina();

LCM_API
void LCM_Assert_(const char *expr_str, int expr, const char *file,
                  const char *func, int line, const char *format, ...)
__attribute__((__format__(__printf__, 6, 7)));

LCM_API
void LCM_Log_(enum LCM_log_level_t log_level, const char *log_type,
              const char *file, const char *func, int line,
              const char *format, ...)
__attribute__((__format__(__printf__, 6, 7)));

#if defined(__cplusplus)
}
#endif

#endif // LCM_LOG_H_
