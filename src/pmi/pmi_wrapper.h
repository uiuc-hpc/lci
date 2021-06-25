#ifndef LCI_PMI_WRAPPER_H
#define LCI_PMI_WRAPPER_H

void lcm_pm_initialize();
int lcm_pm_initialized();
int lcm_pm_rank_me();
int lcm_pm_nranks();
void lcm_pm_publish(char* key, char* value);
void lcm_pm_getname(char* key, char* value);
void lcm_pm_barrier();
void lcm_pm_finalize();
#endif
