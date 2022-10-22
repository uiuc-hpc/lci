/* Copyright (c) 2001-2022, The Ohio State University. All rights
 * reserved.
 *
 * This file is part of the MVAPICH2 software package developed by the
 * team members of The Ohio State University's Network-Based Computing
 * Laboratory (NBCL), headed by Professor Dhabaleswar K. (DK) Panda.
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level MVAPICH2 directory.
 *
 */

#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#elif HAVE_SYSCALL_H
#include <syscall.h>
#endif

#include <unistd.h>

#include "mem_hooks.h"
#include "dreg.h"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

mvapich2_malloc_info_t mvapich2_minfo;
static int mem_hook_init = 0;

#if !(defined(HAVE_SYSCALL) && defined(__NR_munmap))
#include <dlfcn.h>

static pthread_t lock_holder = -1;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int recurse_level = 0;
static int resolving_munmap = 0;
static void * store_buf = NULL;
static size_t store_len = 0;

static void set_real_munmap_ptr()
{
    munmap_t munmap_ptr = (munmap_t) dlsym(RTLD_NEXT, "munmap");
    char* dlerror_str = dlerror();
    LCM_Assert(dlerror_str == NULL, "Error resolving munmap(): %s\n",
               dlerror_str);

    /*
     * The following code tries to detect link error where both static 
     * and dynamic libraries are linked to executable. This link error 
     * is not currently detected by the linker (should it be? I don't know).
     * However, at execution, it produces an infinite recursive loop of 
     * mvapich2_munmap() -> munmap() -> mvapich2_munmap() -> ...
     * that crashes the program. 
     * It is because in this case, the above code picks the wrong munmap() 
     * function from the second library instead of the one from the system.
     */

    void* handle = dlopen("liblci.so", RTLD_LAZY | RTLD_LOCAL);
    dlerror_str = dlerror();
    if(NULL != dlerror_str) {
        // The error in this case can be ignored
        // This is probably because only the shared library is not available. 
        // However, we keep calling dlerror() so it reset the error flag for dl calls.
    }

    if (NULL != handle) {
        /* Shared libraries are in use, otherwise simply proceed. */
        munmap_t mvapich_munmap_ptr = (munmap_t) dlsym(handle, "munmap");
        char* dlerror_str = dlerror();
        LCM_Assert(dlerror_str == NULL,
                   "Error resolving munmap() from libmpich.so: %s\n",
                   dlerror_str);

        LCM_Assert(munmap_ptr != mvapich_munmap_ptr,
                   "Error getting real munmap(). MVAPICH2 cannot run properly.\n"
                   "This error usually means that the program is linked with both static and shared MVAPICH2 libraries.\n"
                   "Please check your Makefile or your link command line.\n");
    }

    mvapich2_minfo.munmap = munmap_ptr;
}

static int lock_hooks(void)
{
    int ret;

    if(pthread_self() == lock_holder) {
        recurse_level++;
        return 0;
    } else {
        if(0 != (ret = pthread_mutex_lock(&lock))) {
            perror("pthread_mutex_lock");
            return ret;
        }
        lock_holder = pthread_self();
        recurse_level++;
    }
    return 0;
}

static int unlock_hooks(void)
{
    int ret;
    if(pthread_self() != lock_holder) {
        return 1;
    } else {
        recurse_level--;
        if(0 == recurse_level) {
            lock_holder = -1;
            if(0 != (ret = pthread_mutex_unlock(&lock))) {
                perror("pthread_mutex_unlock");
                return ret;
            }
        }
    }
    return 0;
}
#else
static int lock_hooks(void) { return 0; }
static int unlock_hooks(void) { return 0; }
#endif
void mvapich2_mem_unhook(void *ptr, size_t size)
{
    if(mem_hook_init && 
            (size > 0) && 
            !mvapich2_minfo.is_mem_hook_finalized) {
        find_and_free_dregs_inside(ptr, size);
    }
}

/* disable compiler optimization for minit() to avoid optimizing out memset */
#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif

/* For clang we have to use __attribute__((optnone)) to disable optimizing out of 
 * calloc and valloc calls while leave the GCC opimize ("O0") to take care of 
 * other compilers that use GCC backend. In future, if need arises we may need 
 * compiler specific disabling of optimizations. 
 */
#ifdef __clang__ 
__attribute__((optnone)) int mvapich2_minit()
#else
int mvapich2_minit()
#endif
{
    assert(0 == mem_hook_init);

    if(lock_hooks()) {
        return 1;
    }

    memset(&mvapich2_minfo, 0, sizeof(mvapich2_malloc_info_t));

    if(!(mvapich2_minfo.is_our_malloc &&
            mvapich2_minfo.is_our_calloc &&
            mvapich2_minfo.is_our_realloc &&
            mvapich2_minfo.is_our_valloc &&
            mvapich2_minfo.is_our_memalign &&
            mvapich2_minfo.is_our_free)) {
        unlock_hooks();
        return 1;
    }

#if !(defined(HAVE_SYSCALL) && defined(__NR_munmap))
    dlerror(); /* Clear the error string */
    resolving_munmap = 1;
    set_real_munmap_ptr();
    resolving_munmap = 0;
    if(NULL != store_buf) {
        mvapich2_minfo.munmap(store_buf, store_len);
        store_buf = NULL; store_len = 0;
    }
#endif

    mem_hook_init = 1;

    if(unlock_hooks()) {
        return 1;
    }

    return 0;
}

/* return to default optimization mode */
#ifdef __GNUC__
#pragma GCC pop_options
#endif

void mvapich2_mfin()
{
    if (mem_hook_init) {
        mvapich2_minfo.is_mem_hook_finalized = 1;
        mem_hook_init = 0;
    }
}

int mvapich2_munmap(void *buf, size_t len)
{
    if(lock_hooks()) {
        return 1;
    }

#if !(defined(HAVE_SYSCALL) && defined(__NR_munmap))
    if(!mvapich2_minfo.munmap &&
            !resolving_munmap) {
        resolving_munmap = 1;
        set_real_munmap_ptr();
        resolving_munmap = 0;
        if(NULL != store_buf) {
            /* resolved munmap ptr successfully,
             * but in the meantime successive
             * stack frame stored a ptr to
             * be really munmap'd. do it now. */

            /* additional note: since munmap ptr
             * was not resolved, therefore we must
             * not have inited mem hooks, i.e.
             * assert(0 == mem_hook_init); therefore
             * this memory does not need to be unhooked
             * (since no MPI call has been issued yet) */

            mvapich2_minfo.munmap(store_buf, store_len);
            store_buf = NULL; store_len = 0;
        }
    }

    if(!mvapich2_minfo.munmap &&
            resolving_munmap) {
        /* prev stack frame is resolving
         * munmap ptr. 
         * store the ptr to be munmap'd
         * for now and return */
        store_buf = buf;
        store_len = len;
        if(unlock_hooks()) {
            return 1;
        }
        return 0;
    }
#endif
    if(mem_hook_init &&
            !mvapich2_minfo.is_mem_hook_finalized) {
        mvapich2_mem_unhook(buf, len);
    }

    if(unlock_hooks()) {
        return 1;
    }

#if !(defined(HAVE_SYSCALL) && defined(__NR_munmap))
    return mvapich2_minfo.munmap(buf, len);
#else
    return syscall(__NR_munmap, buf, len);
#endif
}

int munmap(void *buf, size_t len)
{
    return mvapich2_munmap(buf, len);
}

#if !defined(DISABLE_TRAP_SBRK)
void *mvapich2_sbrk(intptr_t delta)
{
    if (delta < 0) {

        void *current_brk = sbrk(0);

        mvapich2_mem_unhook((void *)
                ((uintptr_t) current_brk + delta), -delta);

        /* -delta is actually a +ve number */
    }

    return sbrk(delta);
}
#endif /* !defined(DISABLE_TRAP_SBRK) */
