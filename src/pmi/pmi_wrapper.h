#ifndef LCI_PMI_WRAPPER_H
#define LCI_PMI_WRAPPER_H

#if defined(__cplusplus)
extern "C" {
#endif

void lcm_pm_initialize();
int lcm_pm_initialized();
int lcm_pm_get_rank();
int lcm_pm_get_size();
void lcm_pm_publish(char *key, char *value);
void lcm_pm_getname(char *key, char *value);
void lcm_pm_barrier();
void lcm_pm_finalize();

#if defined(__cplusplus)
}
#endif
#endif
