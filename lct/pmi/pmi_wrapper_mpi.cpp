#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pmi_wrapper.hpp"
#include "pmii_archive.hpp"
#include <mpi.h>

namespace lct
{
namespace pmi
{
namespace mpi
{
static int is_initialized = 0;
static int to_finalize_mpi = 0;
static archive::Archive_t l_archive, g_archive;

int check_availability() { return true; }

void initialize()
{
  int mpi_initialized = 0;
  MPI_Initialized(&mpi_initialized);
  if (!mpi_initialized) {
    MPI_Init(nullptr, nullptr);
    to_finalize_mpi = 1;
  }
  archive::init(&l_archive);
  archive::init(&g_archive);
  is_initialized = 1;
}

int initialized() { return is_initialized; }
int get_rank()
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return rank;
}

int get_size()
{
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  return size;
}

void publish(char* key, char* value) { archive::push(&l_archive, key, value); }

void getname(int rank, char* key, char* value)
{
  char* ret = archive::search(&g_archive, key);
  strcpy(value, ret);
}

void barrier()
{
  int rank = get_rank();
  int size = get_size();
  // exchange the counts
  int my_count = l_archive.size;
  int* count_buf = (int*)LCTI_malloc(size * sizeof(int));
  MPI_Allgather(&my_count, 1, MPI_INT, count_buf, 1, MPI_INT, MPI_COMM_WORLD);

  // exchange the data
  int total_count = 0;
  for (int i = 0; i < size; ++i) {
    total_count += count_buf[i];
  }
  char* src_buf =
      (char*)LCTI_malloc(l_archive.size * archive::STRING_LIMIT * 2);
  char* dst_buf = (char*)LCTI_malloc(total_count * archive::STRING_LIMIT * 2);
  // refactor the count_buf
  for (int i = 0; i < size; ++i) {
    count_buf[i] *= archive::STRING_LIMIT * 2;
  }
  // assemble the displs_buf
  int* displs_buf = (int*)LCTI_malloc(sizeof(int) * size);
  int current_displs = 0;
  for (int i = 0; i < size; ++i) {
    displs_buf[i] = current_displs;
    current_displs += count_buf[i];
  }
  // assemble the send buffer
  for (int i = 0; i < l_archive.size; ++i) {
    memcpy(src_buf + 2 * i * archive::STRING_LIMIT, l_archive.ptr[i].key,
           archive::STRING_LIMIT);
    memcpy(src_buf + (i * 2 + 1) * archive::STRING_LIMIT,
           l_archive.ptr[i].value, archive::STRING_LIMIT);
  }
  MPI_Allgatherv(src_buf, my_count * archive::STRING_LIMIT * 2, MPI_BYTE,
                 dst_buf, count_buf, displs_buf, MPI_BYTE, MPI_COMM_WORLD);

  // put the data back the global archive
  for (int i = 0; i < total_count; ++i) {
    archive::push(&g_archive, dst_buf + 2 * i * archive::STRING_LIMIT,
                  dst_buf + (2 * i + 1) * archive::STRING_LIMIT);
  }
  LCTI_free(displs_buf);
  LCTI_free(dst_buf);
  LCTI_free(src_buf);
  LCTI_free(count_buf);
  archive::clear(&l_archive);
}

void finalize()
{
  if (to_finalize_mpi) {
    int mpi_finalized = 0;
    MPI_Finalized(&mpi_finalized);
    if (!mpi_finalized) {
      MPI_Finalize();
    }
  }
  archive::fina(&l_archive);
  archive::fina(&g_archive);
  is_initialized = 0;
}

}  // namespace mpi

void mpi_setup_ops(struct ops_t* ops)
{
  ops->check_availability = mpi::check_availability;
  ops->initialize = mpi::initialize;
  ops->is_initialized = mpi::initialized;
  ops->get_rank = mpi::get_rank;
  ops->get_size = mpi::get_size;
  ops->publish = mpi::publish;
  ops->getname = mpi::getname;
  ops->barrier = mpi::barrier;
  ops->finalize = mpi::finalize;
}
}  // namespace pmi
}  // namespace lct