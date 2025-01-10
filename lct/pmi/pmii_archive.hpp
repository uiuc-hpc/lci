#ifndef LCT_PMII_ARCHIVE_HPP
#define LCT_PMII_ARCHIVE_HPP
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

namespace lct
{
namespace pmi
{
namespace archive
{
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

static void init(Archive_t* archive)
{
  archive->capacity = 8;
  archive->size = 0;
  archive->ptr =
      (Entry_t*)LCTI_malloc(archive->capacity * sizeof(struct Entry_t));
  if (archive->ptr == nullptr) {
    fprintf(stderr, "Cannot get more memory for archive of capacity %d",
            archive->capacity);
    exit(1);
  }
}

static void fina(Archive_t* archive)
{
  if (archive->ptr != nullptr) {
    for (int i = 0; i < archive->size; ++i) {
      LCTI_free(archive->ptr[i].key);
      archive->ptr[i].key = nullptr;
      LCTI_free(archive->ptr[i].value);
      archive->ptr[i].value = nullptr;
    }
    LCTI_free(archive->ptr);
    archive->ptr = nullptr;
  }
  archive->size = 0;
  archive->capacity = 0;
}

static void clear(Archive_t* archive)
{
  for (int i = 0; i < archive->size; ++i) {
    LCTI_free(archive->ptr[i].key);
    archive->ptr[i].key = nullptr;
    LCTI_free(archive->ptr[i].value);
    archive->ptr[i].value = nullptr;
  }
  archive->size = 0;
}

static void push(Archive_t* archive, char* key, char* value)
{
  if (archive->size == archive->capacity) {
    archive->capacity *= 2;
    archive->ptr = (Entry_t*)LCTI_realloc(
        archive->ptr, archive->size * sizeof(struct Entry_t),
        archive->capacity * sizeof(struct Entry_t));
    if (archive->ptr == nullptr) {
      fprintf(stderr, "Cannot get more memory for archive of capacity %d",
              archive->capacity);
      exit(1);
    }
  }
  char* key_in = (char*)LCTI_malloc(STRING_LIMIT);
  strcpy(key_in, key);
  char* value_in = (char*)LCTI_malloc(STRING_LIMIT);
  strcpy(value_in, value);
  archive->ptr[archive->size].key = key_in;
  archive->ptr[archive->size].value = value_in;
  ++archive->size;
}

static char* search(Archive_t* archive, char* key)
{
  for (int i = 0; i < archive->size; ++i) {
    if (strcmp(archive->ptr[i].key, key) == 0) {
      return archive->ptr[i].value;
    }
  }
  return nullptr;
}

}  // namespace archive
}  // namespace pmi
}  // namespace lct

#endif  // LCT_PMII_ARCHIVE_HPP
