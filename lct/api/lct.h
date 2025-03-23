#ifndef LCI_LCT_H
#define LCI_LCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#include "lct_config.h"

#define LCT_API __attribute__((visibility("default")))

#define LCT_likely(x) __builtin_expect(!!(x), 1)
#define LCT_unlikely(x) __builtin_expect(!!(x), 0)

LCT_API void LCT_init();
LCT_API void LCT_fina();

// rank
extern int LCT_rank;
LCT_API void LCT_set_rank(int rank);
LCT_API int LCT_get_rank();

// hostname
extern char LCT_hostname[HOST_NAME_MAX + 1];

// time
typedef int64_t LCT_time_t;
LCT_API LCT_time_t LCT_now();
LCT_API double LCT_time_to_ns(LCT_time_t time);
LCT_API double LCT_time_to_us(LCT_time_t time);
LCT_API double LCT_time_to_ms(LCT_time_t time);
LCT_API double LCT_time_to_s(LCT_time_t time);

// string
LCT_API const char* LCT_str_replace_one(const char* in, const char* from,
                                        const char* to);
LCT_API const char* LCT_str_replace_all(const char* in, const char* from,
                                        const char* to);
typedef struct {
  const char* key;
  int val;
} LCT_dict_str_int_t;
LCT_API int LCT_str_int_search(LCT_dict_str_int_t dict[], int count,
                               const char* key, int default_val, int* val);
LCT_API uint64_t LCT_parse_arg(LCT_dict_str_int_t dict[], int count,
                               const char* key, const char* delimiter);

// thread
LCT_API int LCT_get_thread_id();
LCT_API int LCT_get_nthreads();

// log
typedef void* LCT_log_ctx_t;
LCT_API LCT_log_ctx_t LCT_log_ctx_alloc(const char* const log_levels_[],
                                        int count, int default_log_level,
                                        const char* ctx_name, char* filename,
                                        char* log_level_str, char* whitelist,
                                        char* blacklist);
LCT_API void LCT_log_ctx_free(LCT_log_ctx_t* log_ctx_p);
LCT_API int LCT_log_get_level(LCT_log_ctx_t log_ctx);
#define LCT_Assert(log_ctx, Expr, ...)                                      \
  do {                                                                      \
    if (!(Expr))                                                            \
      LCT_Report_false_assert(log_ctx, #Expr, __FILE__, __func__, __LINE__, \
                              __VA_ARGS__);                                 \
  } while (0)

#define LCT_Log(log_ctx, log_level, log_tag, ...)                     \
  LCT_Log_(log_ctx, log_level, log_tag, __FILE__, __func__, __LINE__, \
           __VA_ARGS__)
#define LCT_Warn(log_ctx, ...) \
  LCT_Log(log_ctx, LCT_LOG_WARN, "warn", __VA_ARGS__)
#define LCT_Logv(log_ctx, log_level, log_tag, format, vargs)                   \
  LCT_Logv_(log_ctx, log_level, log_tag, __FILE__, __func__, __LINE__, format, \
            vargs)
LCT_API void LCT_Report_false_assert(LCT_log_ctx_t log_ctx,
                                     const char* expr_str, const char* file,
                                     const char* func, int line,
                                     const char* format, ...);
LCT_API void LCT_Log_(LCT_log_ctx_t log_ctx, int log_level, const char* log_tag,
                      const char* file, const char* func, int line,
                      const char* format, ...);
LCT_API void LCT_Logv_(LCT_log_ctx_t log_ctx, int log_level,
                       const char* log_tag, const char* file, const char* func,
                       int line, const char* format, va_list vargs);
LCT_API void LCT_Log_flush(LCT_log_ctx_t log_ctx);

#ifdef LCT_DEBUG
#define LCT_DBG_Assert(...) LCT_Assert(__VA_ARGS__)
#define LCT_DBG_Log(...) LCT_Log(__VA_ARGS__)
#define LCT_DBG_Warn(...) LCT_Warn(__VA_ARGS__)
#else
#define LCT_DBG_Assert(...)
#define LCT_DBG_Log(...)
#define LCT_DBG_Warn(...)
#endif

// default log ctx
extern LCT_log_ctx_t LCT_log_ctx_default;
enum LCT_log_level_default_t {
  LCT_LOG_ERROR,
  LCT_LOG_WARN,
  LCT_LOG_DIAG,
  LCT_LOG_INFO,
  LCT_LOG_DEBUG,
  LCT_LOG_TRACE,
  LCT_LOG_MAX
};

// cache
#define LCT_ASSERT_SAME_CACHE_LINE(p1, p2)                                     \
  LCT_Assert(LCT_log_ctx_default,                                              \
             (uintptr_t)p1 / LCT_CACHE_LINE == (uintptr_t)p2 / LCT_CACHE_LINE, \
             "%p and %p is not in the same L1 cache line (%d B)\n", p1, p2,    \
             LCT_CACHE_LINE)
#define LCT_ASSERT_DIFF_CACHE_LINE(p1, p2)                                     \
  LCT_Assert(LCT_log_ctx_default,                                              \
             (uintptr_t)p1 / LCT_CACHE_LINE != (uintptr_t)p2 / LCT_CACHE_LINE, \
             "%p and %p is not in different L1 cache lines (%d B)\n", p1, p2,  \
             LCT_CACHE_LINE)

// Performance Counter
typedef enum {
  LCT_PCOUNTER_NONE,
  LCT_PCOUNTER_COUNTER,
  LCT_PCOUNTER_TREND,
  LCT_PCOUNTER_TIMER,
} LCT_pcounter_type_t;
typedef void* LCT_pcounter_ctx_t;
typedef struct {
  LCT_pcounter_type_t type;
  int idx;
} LCT_pcounter_handle_t;
LCT_API LCT_pcounter_ctx_t LCT_pcounter_ctx_alloc(const char* ctx_name);
LCT_API LCT_pcounter_handle_t
LCT_pcounter_register(LCT_pcounter_ctx_t pcounter_ctx, const char* name,
                      LCT_pcounter_type_t type);
LCT_API void LCT_pcounter_ctx_free(LCT_pcounter_ctx_t* pcounter_ctx);
LCT_API void LCT_pcounter_add(LCT_pcounter_ctx_t pcounter_ctx,
                              LCT_pcounter_handle_t handle, int64_t val);
LCT_API void LCT_pcounter_start(LCT_pcounter_ctx_t pcounter_ctx,
                                LCT_pcounter_handle_t handle);
LCT_API void LCT_pcounter_end(LCT_pcounter_ctx_t pcounter_ctx,
                              LCT_pcounter_handle_t handle);
LCT_API void LCT_pcounter_startt(LCT_pcounter_ctx_t pcounter_ctx,
                                 LCT_pcounter_handle_t handle, LCT_time_t time);
LCT_API void LCT_pcounter_endt(LCT_pcounter_ctx_t pcounter_ctx,
                               LCT_pcounter_handle_t handle, LCT_time_t time);
LCT_API void LCT_pcounter_record(LCT_pcounter_ctx_t pcounter_ctx);
LCT_API void LCT_pcounter_dump(LCT_pcounter_ctx_t pcounter_ctx, FILE* out);

// Data Structures
// Queues
typedef enum {
  LCT_QUEUE_ARRAY_ATOMIC_FAA,
  LCT_QUEUE_ARRAY_ATOMIC_CAS,
  LCT_QUEUE_ARRAY_ATOMIC_BASIC,
  LCT_QUEUE_ARRAY_ST,
  LCT_QUEUE_ARRAY_MUTEX,
  LCT_QUEUE_STD,
  LCT_QUEUE_STD_MUTEX,
  LCT_QUEUE_MS,
  LCT_QUEUE_LCRQ,
  LCT_QUEUE_LPRQ,
  LCT_QUEUE_FAAARRAY,
  LCT_QUEUE_LAZY_INDEX,
} LCT_queue_type_t;
struct LCT_queue_opaque_t;
typedef struct LCT_queue_opaque_t* LCT_queue_t;
LCT_API LCT_queue_t LCT_queue_alloc(LCT_queue_type_t type, size_t length);
LCT_API void LCT_queue_free(LCT_queue_t* queue_p);
LCT_API void LCT_queue_push(LCT_queue_t queue, void* val);
LCT_API void* LCT_queue_pop(LCT_queue_t queue);

// PMI
#define LCT_PMI_STRING_LIMIT 255
LCT_API void LCT_pmi_initialize();
LCT_API int LCT_pmi_initialized();
LCT_API int LCT_pmi_get_rank();
LCT_API int LCT_pmi_get_size();
LCT_API void LCT_pmi_publish(char* key, char* value);
LCT_API void LCT_pmi_getname(int rank, char* key, char* value);
LCT_API void LCT_pmi_barrier();
LCT_API void LCT_pmi_finalize();

// Args Parsing
typedef void* LCT_args_parser_t;
LCT_API LCT_args_parser_t LCT_args_parser_alloc();
LCT_API void LCT_args_parser_free(LCT_args_parser_t parser);
LCT_API void LCT_args_parser_add(LCT_args_parser_t parser, const char* name,
                                 int has_arg, int* ptr);
LCT_API void LCT_args_parser_add_dict(LCT_args_parser_t parser,
                                      const char* name, int has_arg, int* ptr,
                                      LCT_dict_str_int_t dict[], int count);
LCT_API void LCT_args_parser_parse(LCT_args_parser_t parser, int argc,
                                   char* argv[]);
LCT_API void LCT_args_parser_print(LCT_args_parser_t parser, bool verbose);

// Thread barrier
typedef void* LCT_tbarrier_t;
LCT_API LCT_tbarrier_t LCT_tbarrier_alloc(int nthreads);
LCT_API void LCT_tbarrier_free(LCT_tbarrier_t* tbarrier_p);
LCT_API int64_t LCT_tbarrier_arrive(LCT_tbarrier_t tbarrier);
LCT_API bool LCT_tbarrier_test(LCT_tbarrier_t tbarrier, int64_t ticket);
LCT_API void LCT_tbarrier_wait(LCT_tbarrier_t tbarrier, int64_t ticket);
LCT_API void LCT_tbarrier_arrive_and_wait(LCT_tbarrier_t tbarrier);

// File IO
LCT_API ssize_t LCT_read_file(char* buffer, size_t max,
                              const char* filename_fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  // LCI_LCT_H
