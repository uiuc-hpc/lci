#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "pmi_wrapper.hpp"

struct lct::pmi::ops_t lcti_pmi_ops;

void LCT_pmi_initialize()
{
  std::string backends_str;
  {
    char* p = getenv("LCT_PMI_BACKEND");
    if (p == nullptr)
      backends_str = LCT_PMI_BACKEND_DEFAULT;
    else
      backends_str = p;
  }

  char* str = strdup(backends_str.c_str());
  char* word;
  char* rest = str;
  bool found_valid_backend = false;
  while ((word = strtok_r(rest, " ;,", &rest))) {
    if (strcmp(word, "local") == 0) {
      lct::pmi::local_setup_ops(&lcti_pmi_ops);
    } else if (strcmp(word, "file") == 0) {
      lct::pmi::file_setup_ops(&lcti_pmi_ops);
    } else if (strcmp(word, "pmi1") == 0) {
#ifdef LCT_PMI_BACKEND_ENABLE_PMI1
      lct::pmi::pmi1_setup_ops(&lcti_pmi_ops);
#else
      LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi",
              "LCT is not compiled with the %s backend. Skip.\n", word);
      continue;
#endif
    } else if (strcmp(word, "pmi2") == 0) {
#ifdef LCT_PMI_BACKEND_ENABLE_PMI2
      lct::pmi::pmi2_setup_ops(&lcti_pmi_ops);
#else
      LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi",
              "LCT is not compiled with the %s backend. Skip.\n", word);
      continue;
#endif
    } else if (strcmp(word, "pmix") == 0) {
#ifdef LCT_PMI_BACKEND_ENABLE_PMIX
      lct::pmi::pmix_setup_ops(&lcti_pmi_ops);
#else
      LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi",
              "LCT is not compiled with the %s backend. Skip.\n", word);
      continue;
#endif
    } else if (strcmp(word, "mpi") == 0) {
#ifdef LCT_PMI_BACKEND_ENABLE_MPI
      lct::pmi::mpi_setup_ops(&lcti_pmi_ops);
#else
      LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi",
              "LCT is not compiled with the %s backend. Skip.\n", word);
      continue;
#endif
    } else
      LCT_Assert(LCT_log_ctx_default, false,
                 "Unknown env LCM_PMI_BACKEND (%s against "
                 "local|file|pmi1|pmi2|pmix|mpi).\n",
                 word);
    if (lcti_pmi_ops.check_availability()) {
      LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi",
              "Use %s as the PMI backend.\n", word);
      found_valid_backend = true;
      break;
    } else {
      LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi",
              "The PMI backend %s is not available.\n", word);
      continue;
    }
  }
  free(str);
  LCT_Assert(LCT_log_ctx_default, found_valid_backend,
             "Tried [%s]. Did not find valid PMI backend! Give up!\n",
             backends_str.c_str());
  lcti_pmi_ops.initialize();
  LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi",
          "This process is initialized as %d/%d.\n", LCT_pmi_get_rank(),
          LCT_pmi_get_size());
}

int LCT_pmi_initialized() { return lcti_pmi_ops.is_initialized(); }
int LCT_pmi_get_rank() { return lcti_pmi_ops.get_rank(); }
int LCT_pmi_get_size() { return lcti_pmi_ops.get_size(); }
void LCT_pmi_publish(char* key, char* value)
{
  LCT_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "pmi", "publish %s %s\n", key,
          value);
  lcti_pmi_ops.publish(key, value);
}
void LCT_pmi_getname(int rank, char* key, char* value)
{
  lcti_pmi_ops.getname(rank, key, value);
  LCT_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "pmi", "getname %d %s %s\n", rank,
          key, value);
}
void LCT_pmi_barrier()
{
  LCT_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "pmi", "enter pmi barrier\n");
  lcti_pmi_ops.barrier();
  LCT_Log(LCT_log_ctx_default, LCT_LOG_DEBUG, "pmi", "leave pmi barrier\n");
}
void LCT_pmi_finalize() { lcti_pmi_ops.finalize(); }