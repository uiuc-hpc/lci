#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pmi_wrapper.h"
#include "pmii_archive.h"
#include <mpi.h>

static int initialized = 0;
static int to_finalize_mpi = 0;
static Archive_t l_archive, g_archive;

int lcm_pm_mpi_check_availability() { return true; }

void lcm_pm_mpi_initialize()
{
  int mpi_initialized = 0;
  MPI_Initialized(&mpi_initialized);
  if (!mpi_initialized) {
    MPI_Init(NULL, NULL);
    to_finalize_mpi = 1;
  }
  archive_init(&l_archive);
  archive_init(&g_archive);
  initialized = 1;
}

int lcm_pm_mpi_initialized() { return initialized; }
int lcm_pm_mpi_get_rank()
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return rank;
}

int lcm_pm_mpi_get_size()
{
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  return size;
}

void lcm_pm_mpi_publish(char* key, char* value)
{
  archive_push(&l_archive, key, value);
}

void lcm_pm_mpi_getname(int rank, char* key, char* value)
{
  char* ret = archive_search(&g_archive, key);
  strcpy(value, ret);
}

void lcm_pm_mpi_barrier()
{
  int rank = lcm_pm_mpi_get_rank();
  int size = lcm_pm_mpi_get_size();
  // exchange the counts
  int my_count = l_archive.size;
  int* count_buf = malloc(size * sizeof(int));
  MPI_Allgather(&my_count, 1, MPI_INT, count_buf, 1, MPI_INT, MPI_COMM_WORLD);

  // exchange the data
  int total_count = 0;
  for (int i = 0; i < size; ++i) {
    total_count += count_buf[i];
  }
  char* src_buf = malloc(l_archive.size * STRING_LIMIT * 2);
  char* dst_buf = malloc(total_count * STRING_LIMIT * 2);
  // refactor the count_buf
  for (int i = 0; i < size; ++i) {
    count_buf[i] *= STRING_LIMIT * 2;
  }
  // assemble the displs_buf
  int* displs_buf = malloc(sizeof(int) * size);
  int current_displs = 0;
  for (int i = 0; i < size; ++i) {
    displs_buf[i] = current_displs;
    current_displs += count_buf[i];
  }
  // assemble the send buffer
  for (int i = 0; i < l_archive.size; ++i) {
    memcpy(src_buf + 2 * i * STRING_LIMIT, l_archive.ptr[i].key, STRING_LIMIT);
    memcpy(src_buf + (i * 2 + 1) * STRING_LIMIT, l_archive.ptr[i].value,
           STRING_LIMIT);
  }
  MPI_Allgatherv(src_buf, my_count * STRING_LIMIT * 2, MPI_BYTE, dst_buf,
                 count_buf, displs_buf, MPI_BYTE, MPI_COMM_WORLD);

  // put the data back the global archive
  for (int i = 0; i < total_count; ++i) {
    archive_push(&g_archive, dst_buf + 2 * i * STRING_LIMIT,
                 dst_buf + (2 * i + 1) * STRING_LIMIT);
  }
  free(displs_buf);
  free(dst_buf);
  free(src_buf);
  free(count_buf);
  archive_clear(&l_archive);
}

void lcm_pm_mpi_finalize()
{
  if (to_finalize_mpi) {
    int mpi_finalized = 0;
    MPI_Finalized(&mpi_finalized);
    if (!mpi_finalized) {
      MPI_Finalize();
    }
  }
  archive_fina(&l_archive);
  archive_fina(&g_archive);
  initialized = 0;
}

void lcm_pm_mpi_setup_ops(struct LCM_PM_ops_t* ops)
{
  ops->check_availability = lcm_pm_mpi_check_availability;
  ops->initialize = lcm_pm_mpi_initialize;
  ops->is_initialized = lcm_pm_mpi_initialized;
  ops->get_rank = lcm_pm_mpi_get_rank;
  ops->get_size = lcm_pm_mpi_get_size;
  ops->publish = lcm_pm_mpi_publish;
  ops->getname = lcm_pm_mpi_getname;
  ops->barrier = lcm_pm_mpi_barrier;
  ops->finalize = lcm_pm_mpi_finalize;
}