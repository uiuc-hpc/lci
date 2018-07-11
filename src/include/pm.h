#ifndef _LC_PM_H_
#define _LC_PM_H_

#include "pmi.h"

void lc_pm_master_init(int* size, int* rank, char* name);
void lc_pm_publish(int prank, int erank, char* value);
void lc_pm_getname(int prank, int erank, char* value);
#endif
