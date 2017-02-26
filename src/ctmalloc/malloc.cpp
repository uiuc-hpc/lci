#include "config.h"

#include "wtf/PartitionAlloc.h"

#include <string.h>

static PartitionAllocatorGeneric partition;
static bool initialized;

extern "C" {

void __libc_free(void* ptr);
void* __libc_malloc(size_t size);
void* __libc_realloc(void* ptr, size_t size);
void* __libc_calloc(size_t, size_t size);

#include <stdio.h>

void* malloc(size_t size)
{
    if (UNLIKELY(!initialized)) {
        initialized = true;
        partition.init();
    }
    return partitionAllocGeneric(partition.root(), size);
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
  *memptr = malloc(size);
  return 0;
}

void* valloc(size_t size)
{
  return malloc(size);
}

void* memalign(size_t alignment, size_t size)
{
  return malloc(size);
}

void free(void* ptr)
{
    if (UNLIKELY(!initialized)) {
        initialized = true;
        partition.init();
    }

    partitionFreeGeneric(partition.root(), ptr);
}

void* realloc(void* ptr, size_t size)
{
    if (UNLIKELY(!initialized)) {
        initialized = true;
        partition.init();
    }
    if (UNLIKELY(!ptr)) {
        return partitionAllocGeneric(partition.root(), size);
    }
    if (UNLIKELY(!size)) {
        partitionFreeGeneric(partition.root(), ptr);
        return 0;
    }
    return partitionReallocGeneric(partition.root(), ptr, size);
}

void* calloc(size_t nmemb, size_t size)
{
    void* ret;
    size_t real_size = nmemb * size;
    if (UNLIKELY(!initialized)) {
        initialized = true;
        partition.init();
    }
    RELEASE_ASSERT(!nmemb || real_size / nmemb == size);
    ret = partitionAllocGeneric(partition.root(), real_size);
    memset(ret, '\0', real_size);
    return ret;
}

}
