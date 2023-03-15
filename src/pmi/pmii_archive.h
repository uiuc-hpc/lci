#ifndef LCI_PMII_ARCHIVE_H
#define LCI_PMII_ARCHIVE_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct Entry_t {
  char* key;
  char* value;
} Entry_t;

typedef struct Archive_t {
  Entry_t* ptr;
  int size;
  int capacity;
} Archive_t;

static const int STRING_LIMIT = 256;

static void archive_init(Archive_t* archive)
{
  archive->capacity = 8;
  archive->size = 0;
  archive->ptr = malloc(archive->capacity * sizeof(struct Entry_t));
  if (archive->ptr == NULL) {
    fprintf(stderr, "Cannot get more memory for archive of capacity %d",
            archive->capacity);
    exit(1);
  }
}

static void archive_fina(Archive_t* archive)
{
  if (archive->ptr != NULL) {
    for (int i = 0; i < archive->size; ++i) {
      free(archive->ptr[i].key);
      archive->ptr[i].key = NULL;
      free(archive->ptr[i].value);
      archive->ptr[i].value = NULL;
    }
    free(archive->ptr);
    archive->ptr = NULL;
  }
  archive->size = 0;
  archive->capacity = 0;
}

static void archive_clear(Archive_t* archive)
{
  for (int i = 0; i < archive->size; ++i) {
    free(archive->ptr[i].key);
    archive->ptr[i].key = NULL;
    free(archive->ptr[i].value);
    archive->ptr[i].value = NULL;
  }
  archive->size = 0;
}

static void archive_push(Archive_t* archive, char* key, char* value)
{
  if (archive->size == archive->capacity) {
    archive->capacity *= 2;
    archive->ptr =
        realloc(archive->ptr, archive->capacity * sizeof(struct Entry_t));
    if (archive->ptr == NULL) {
      fprintf(stderr, "Cannot get more memory for archive of capacity %d",
              archive->capacity);
      exit(1);
    }
  }
  char* key_in = malloc(STRING_LIMIT);
  strcpy(key_in, key);
  char* value_in = malloc(STRING_LIMIT);
  strcpy(value_in, value);
  archive->ptr[archive->size].key = key_in;
  archive->ptr[archive->size].value = value_in;
  ++archive->size;
}

static char* archive_search(Archive_t* archive, char* key)
{
  for (int i = 0; i < archive->size; ++i) {
    if (strcmp(archive->ptr[i].key, key) == 0) {
      return archive->ptr[i].value;
    }
  }
  return NULL;
}

#endif  // LCI_PMII_ARCHIVE_H
