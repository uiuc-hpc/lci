#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>

#include "lc.h"

#define container_of(ptr, type, member) ((type *)((uint8_t *)(ptr) - offsetof(type, member)))

static struct {
    pthread_cond_t  cond;
    pthread_mutex_t mutex;
    bool            status;
} progress_start = { PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, false };
static pthread_t progress_thread_id;
static atomic_bool progress_thread_stop = false;
static int current_tag = 0;

static lc_ep default_ep;
static lc_ep put_am_ep;
static lc_ep put_ep;

static int ep_rank = 0;
static int ep_size = 0;

static size_t msg_count = 0;
static size_t msg_size = 0;
static size_t total_msg_count = 0;

static size_t send_count = 0;
static size_t recv_count = 0;

static uint8_t *recv_data = NULL;

#define RETRY(lci_call) do { } while (LC_OK != (lci_call))

typedef struct list_item_s list_item_t;
struct list_item_s {
    list_item_t *next;
    list_item_t *prev;
};

typedef struct list_s list_t;
struct list_s {
    list_item_t ghost;
    atomic_flag lock;
};

#define LIST_HEAD(LIST) ((LIST)->ghost.next)
#define LIST_TAIL(LIST) ((LIST)->ghost.prev)
#define LIST_GHOST(LIST) (&((list)->ghost))

static inline void list_init(list_t *list)
{
    LIST_HEAD(list) = LIST_GHOST(list);
    LIST_TAIL(list) = LIST_GHOST(list);
    atomic_flag_clear_explicit(&list->lock, memory_order_release);
}

static inline void list_unlock(list_t *list)
{
    atomic_flag_clear_explicit(&list->lock, memory_order_release);
}

static inline void list_lock(list_t *list)
{
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 };
    while (atomic_flag_test_and_set_explicit(&list->lock, memory_order_acq_rel))
        nanosleep(&ts, NULL);
}

static inline bool list_trylock(list_t *list)
{
    return !atomic_flag_test_and_set_explicit(&list->lock, memory_order_acq_rel);
}

static inline bool list_empty(list_t *list)
{
    return LIST_HEAD(list) == LIST_GHOST(list);
}

static inline void list_push_front(list_t *list, list_item_t *item)
{
    item->prev = LIST_GHOST(list);
    item->next = LIST_HEAD(list);
    LIST_HEAD(list)->prev = item;
    LIST_HEAD(list) = item;
}

static inline void list_push_back(list_t *list, list_item_t *item)
{
    item->next = LIST_GHOST(list);
    item->prev = LIST_TAIL(list);
    LIST_TAIL(list)->next = item;
    LIST_TAIL(list) = item;
}

static inline list_item_t * list_pop_front(list_t *list)
{
    list_item_t *item = LIST_HEAD(list);
    LIST_HEAD(list) = item->next;
    LIST_HEAD(list)->prev = LIST_GHOST(list);
    return LIST_GHOST(list) != item ? item : NULL;
}

static inline list_item_t * list_unchain(list_t *list)
{
    if (list_empty(list))
        return NULL;
    list_item_t *head = LIST_HEAD(list);
    list_item_t *tail = LIST_TAIL(list);
    LIST_HEAD(list) = LIST_GHOST(list);
    LIST_TAIL(list) = LIST_GHOST(list);
    head->prev = tail;
    tail->next = head;
    return head;
}

static inline void list_chain_back(list_t *list, list_item_t *head)
{
    list_item_t *tail = head->prev;
    tail->next = LIST_GHOST(list);
    head->prev = LIST_TAIL(list);
    LIST_TAIL(list)->next = head;
    LIST_TAIL(list) = tail;
}

typedef struct req_handle_s {
    list_item_t list_item;
    lc_req      req;
} req_handle_t;

typedef enum {
    PUT_TARGET_HANDSHAKE,
    PUT_TARGET,
    PUT_ORIGIN,
} cb_handle_type_t;

typedef struct cb_handle_s {
    list_item_t list_item;
    struct {
        void *data;
        int tag;
        int remote;
    } args;
    cb_handle_type_t type;
    req_handle_t *req_handle;
} cb_handle_t;

static list_t progress_list;
static list_t shared_list;
static list_t comm_list;
static list_t free_handle_list;
static list_t free_req_list;

static inline void bind_thread(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(cpu_set_t), &set);
}

static void * progress_thread(void *arg)
{
    int *cpu = arg;
    bind_thread(*cpu);

    /* signal init */
    pthread_mutex_lock(&progress_start.mutex);
    progress_start.status = true;
    pthread_cond_signal(&progress_start.cond);
    pthread_mutex_unlock(&progress_start.mutex);

    /* loop until told to stop */
    while (!atomic_load_explicit(&progress_thread_stop, memory_order_acquire)) {
        size_t progress_count = 0;
        /* progress until nothing progresses */
        while (lc_progress(0))
            continue;

        if (!list_empty(&progress_list) && list_trylock(&shared_list)) {
            list_chain_back(&shared_list, list_unchain(&progress_list));
            list_unlock(&shared_list);
        }
    }

    free(arg);

    return NULL;
}

static inline void init_progress_thread(int cpu)
{
    int *cpu_arg = malloc(sizeof(int));
    *cpu_arg = cpu;

    /* lock mutex before starting thread */
    pthread_mutex_lock(&progress_start.mutex);
    /* ensure progress_thread_stop == false */
    atomic_store_explicit(&progress_thread_stop, false, memory_order_release);
    /* start thread */
    pthread_create(&progress_thread_id, NULL, progress_thread, cpu_arg);
    /* wait until thread started */
    while (!progress_start.status)
        pthread_cond_wait(&progress_start.cond, &progress_start.mutex);
    /* unlock mutex */
    pthread_mutex_unlock(&progress_start.mutex);
}

static inline void fini_progress_thread(void)
{
    void *progress_retval = NULL;
    atomic_store_explicit(&progress_thread_stop, true, memory_order_release);
    pthread_join(progress_thread_id, &progress_retval);
}

static inline cb_handle_t * handle_alloc(void)
{
    list_lock(&free_handle_list);
    list_item_t *item = list_pop_front(&free_handle_list);
    cb_handle_t *handle = container_of(item, cb_handle_t, list_item);
    list_unlock(&free_handle_list);
    if (!handle) {
        handle = malloc(sizeof(*handle));
    }
    return handle;
}

static inline void handle_free(cb_handle_t *handle)
{
    list_lock(&free_handle_list);
    list_push_front(&free_handle_list, &handle->list_item);
    list_unlock(&free_handle_list);
}

static inline void handle_list_free(void)
{
    list_lock(&free_handle_list);
    for (list_item_t *item = list_pop_front(&free_handle_list);
            item != NULL;
            item = list_pop_front(&free_handle_list)) {
        cb_handle_t *handle = container_of(item, cb_handle_t, list_item);
        free(handle);
    }
    list_unlock(&free_handle_list);
}

static inline req_handle_t * req_alloc(void)
{
    list_lock(&free_req_list);
    list_item_t *item = list_pop_front(&free_req_list);
    req_handle_t *handle = container_of(item, req_handle_t, list_item);
    list_unlock(&free_req_list);
    if (!handle) {
        handle = malloc(sizeof(*handle));
    }
    return handle;
}

static inline void req_free(req_handle_t *handle)
{
    list_lock(&free_req_list);
    list_push_front(&free_req_list, &handle->list_item);
    list_unlock(&free_req_list);
}

static inline void req_list_free(void)
{
    list_lock(&free_req_list);
    for (list_item_t *item = list_pop_front(&free_req_list);
            item != NULL;
            item = list_pop_front(&free_req_list)) {
        req_handle_t *handle = container_of(item, req_handle_t, list_item);
        free(handle);
    }
    list_unlock(&free_req_list);
}

static inline void * null_alloc(size_t size, void **ctx) { return NULL; }

static inline void put_target_handshake_handler(lc_req *req)
{
    cb_handle_t *handle = handle_alloc();
    handle->args.tag    = req->meta;
    handle->args.remote = req->rank;
    handle->type        = PUT_TARGET_HANDSHAKE;
    list_push_back(&progress_list, &handle->list_item);
}

static inline void put_target_handler(lc_req *req)
{
    cb_handle_t *handle = req->ctx;
    if (pthread_equal(progress_thread_id, pthread_self())) {
        list_push_back(&progress_list, &handle->list_item);
    } else {
        list_push_back(&comm_list, &handle->list_item);
    }
}

static inline void put_origin_handler(void *ctx)
{
    cb_handle_t *handle = ctx;
    if (pthread_equal(progress_thread_id, pthread_self())) {
        list_push_back(&progress_list, &handle->list_item);
    } else {
        list_push_back(&comm_list, &handle->list_item);
    }
}

static inline void init_ep(void)
{
    lc_opt opt = { .dev = 0 };

    opt.desc = LC_DYN_AM;
    opt.alloc = null_alloc;
    opt.handler = put_target_handshake_handler;
    lc_ep_dup(&opt, default_ep, &put_am_ep);

    opt.desc = LC_EXP_AM;
    opt.alloc = NULL;
    opt.handler = put_target_handler;
    lc_ep_dup(&opt, default_ep, &put_ep);
}

static inline void cb_progress(void)
{
    list_item_t *ring = NULL;
    req_handle_t *req_handle = NULL;

    /* push back to private callback fifo if we got lock and list nonempty */
    /* unchain list if we get lock */
    if (list_trylock(&shared_list)) {
        ring = list_unchain(&shared_list);
        list_unlock(&shared_list);
    }

    /* chain back if ring not NULL i.e. got lock AND shared fifo not empty */
    if (NULL != ring) {
        list_chain_back(&comm_list, ring);
    }

    for (list_item_t *item = list_pop_front(&comm_list);
            item != NULL;
            item = list_pop_front(&comm_list)) {
        cb_handle_t *handle = container_of(item, cb_handle_t, list_item);
        switch (handle->type) {
        case PUT_ORIGIN:
            send_count++;
            handle_free(handle);
            break;
        case PUT_TARGET_HANDSHAKE:
            req_handle = req_alloc();
            req_handle->req.ctx = handle;
            handle->req_handle = req_handle;
            handle->args.data = recv_data;
            handle->type      = PUT_TARGET;
            RETRY(lc_recv(recv_data, msg_size,
                          handle->args.remote, handle->args.tag,
                          put_ep, &req_handle->req));
            recv_data += msg_size;
            break;
        case PUT_TARGET:
            recv_count++;
            req_free(handle->req_handle);
            handle_free(handle);
            break;
        default:
            fprintf(stderr, "handle->type is wrong\n");
            abort();
        }
    }
}

static inline void do_send(void *data, size_t size, int remote)
{
    int tag = current_tag++;
    cb_handle_t *handle = handle_alloc();
    handle->args.tag = tag;
    handle->args.remote = remote;
    handle->args.data = data;
    handle->type = PUT_ORIGIN;
    RETRY(lc_sendm(NULL, 0, remote, tag, put_am_ep));
    RETRY(lc_send(data, size, remote, tag, put_ep, put_origin_handler, handle));
}

static inline void do_all_send(void)
{
    void *buffer = malloc(msg_size * msg_count);
    uint8_t *data = buffer;
    for (size_t i = 0; i < msg_count; i++) {
        do_send(data, msg_size, 0);
        data += msg_size;
    }
    while (send_count < msg_count) {
        cb_progress();
    }
    free(buffer);
}

static inline void do_all_recv(void)
{
    void *buffer = malloc(msg_size * total_msg_count);
    recv_data = buffer;
    while (recv_count < total_msg_count) {
        cb_progress();
    }
    recv_data = NULL;
    free(buffer);
}

static inline void scan_size(const char *restrict buffer, size_t *dest)
{
    char suffix = '\0';
    int ret = sscanf(buffer, "%zu%c", dest, &suffix);
    if (2 == ret) {
        if        ('G' == suffix) {
            *dest *= (1UL << 30);
        } else if ('M' == suffix) {
            *dest *= (1UL << 20);
        } else if ('K' == suffix) {
            *dest *= (1UL << 10);
        } else {
            printf("bad suffix: \'%c\'\n", suffix);
            exit(EXIT_FAILURE);
        }
    } else if (1 != ret) {
        printf("bad input: %s\n", buffer);
        exit(EXIT_FAILURE);
    }
}

static inline void parse_args(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "needs count and size\n");
        exit(EXIT_FAILURE);
    }

    scan_size(argv[1], &msg_count);
    scan_size(argv[2], &msg_size);
    printf("%zu %zu\n", msg_count, msg_size);
}

int main(int argc, char *argv[])
{
    parse_args(argc, argv);

    list_init(&progress_list);
    list_init(&shared_list);
    list_init(&comm_list);
    list_init(&free_handle_list);
    list_init(&free_req_list);

    lc_init(1, &default_ep);
    lc_get_num_proc(&ep_size);
    lc_get_proc_num(&ep_rank);
    init_ep();

    total_msg_count = msg_count * (ep_size - 1);

    bind_thread(1);
    init_progress_thread(0);

    if (ep_rank == 0) {
        do_all_recv();
    } else {
        do_all_send();
    }

    handle_list_free();
    req_list_free();

    fini_progress_thread();
    lc_finalize();
}
