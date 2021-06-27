#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pmi_wrapper.h"
#include <mpi.h>

typedef struct Entry_t {
  char *key;
  char *value;
} Entry_t;

typedef struct Archive_t {
  Entry_t *ptr;
  int size;
  int capacity;
} Archive_t;
Archive_t l_archive, g_archive;
const int STRING_LIMIT=256;

void archive_init(Archive_t *archive) {
  archive->capacity = 8;
  archive->size = 0;
  archive->ptr = malloc(archive->capacity * sizeof(struct Entry_t));
  if (archive->ptr == NULL) {
    fprintf(stderr, "Cannot get more memory for archive of capacity %d",
            archive->capacity);
    exit(1);
  }
}

void archive_fina(Archive_t *archive) {
  for (int i = 0; i < archive->size; ++i) {
    free(archive->ptr[i].key);
    free(archive->ptr[i].value);
  }
  free(archive->ptr);
  archive->ptr = NULL;
  archive->size = 0;
  archive->capacity = 0;
}

void archive_clear(Archive_t *archive) {
  archive->size = 0;
}

void archive_push(Archive_t *archive, char *key, char *value) {
  if (archive->size == archive->capacity) {
    archive->capacity *= 2;
    archive->ptr = realloc(archive->ptr, archive->capacity * sizeof(struct Entry_t));
    if (archive->ptr == NULL) {
      fprintf(stderr, "Cannot get more memory for archive of capacity %d",
              archive->capacity);
      exit(1);
    }
  }
  char *key_in = malloc(STRING_LIMIT);
  strcpy(key_in, key);
  char *value_in = malloc(STRING_LIMIT);
  strcpy(value_in, value);
  archive->ptr[archive->size].key = key_in;
  archive->ptr[archive->size].value = value_in;
  ++archive->size;
}

char* archive_search(Archive_t *archive, char *key) {
  for (int i = 0; i < archive->size; ++i) {
    if (strcmp(archive->ptr[i].key, key) == 0) {
      return archive->ptr[i].value;
    }
  }
  return NULL;
}

void lcm_pm_initialize()
{
  MPI_Init(NULL, NULL);
  archive_init(&l_archive);
  archive_init(&g_archive);
}

int lcm_pm_initialized() {
  int flag;
  MPI_Initialized(&flag);
  return flag;
}
int lcm_pm_get_rank() {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return rank;
}

int lcm_pm_get_size() {
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  return size;
}

void lcm_pm_publish(char* key, char* value)
{
  archive_push(&l_archive, key, value);
}

void lcm_pm_getname(char* key, char* value)
{
  char *ret = archive_search(&g_archive, key);
  strcpy(value, ret);
}

void lcm_pm_barrier() {
  int rank = lcm_pm_get_rank();
  int size = lcm_pm_get_size();
  // exchange the counts
  int my_count = l_archive.size;
  int *count_buf = malloc(size * sizeof(int));
  MPI_Allgather(&my_count, 1, MPI_INT, count_buf, 1, MPI_INT, MPI_COMM_WORLD);

  // exchange the data
  int total_count = 0;
  for (int i = 0; i < size; ++i) {
    total_count += count_buf[i];
  }
  char *src_buf = malloc(l_archive.size * STRING_LIMIT * 2);
  char *dst_buf = malloc(total_count * STRING_LIMIT * 2);
  // refactor the count_buf
  for (int i = 0; i < size; ++i) {
    count_buf[i] *= STRING_LIMIT * 2;
  }
  // assemble the displs_buf
  int *displs_buf = malloc(sizeof(int) * size);
  int current_displs = 0;
  for (int i = 0; i < size; ++i) {
    displs_buf[i] = current_displs;
    current_displs += count_buf[i];
  }
  // assemble the send buffer
  for (int i = 0; i < l_archive.size; ++i) {
    memcpy(src_buf + 2 * i * STRING_LIMIT, l_archive.ptr[i].key, STRING_LIMIT);
    memcpy(src_buf + (i * 2 + 1) * STRING_LIMIT, l_archive.ptr[i].value, STRING_LIMIT);
  }
  MPI_Allgatherv(src_buf, my_count * STRING_LIMIT * 2, MPI_BYTE, dst_buf,
                 count_buf, displs_buf, MPI_BYTE, MPI_COMM_WORLD);

  // put the data back the global archive
  for (int i = 0; i < total_count; ++i) {
    archive_push(&g_archive, dst_buf + 2 * i * STRING_LIMIT, dst_buf + (2 * i + 1) * STRING_LIMIT);
  }
  free(displs_buf);
  free(dst_buf);
  free(src_buf);
  free(count_buf);
  archive_clear(&l_archive);
}

void lcm_pm_finalize() {
  archive_fina(&l_archive);
  archive_fina(&g_archive);
  MPI_Finalize();
}