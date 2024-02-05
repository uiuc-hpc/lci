#ifndef SERVER_UCX_H_
#define SERVER_UCX_H_

#include <ucp/api/ucp.h>

struct LCISI_endpoint_t;

typedef struct __attribute__((aligned(LCI_CACHE_LINE))) LCISI_memh_wrapper {
  ucp_mem_h memh;
  ucp_context_h context;
} LCISI_memh_wrapper;

typedef struct __attribute__((aligned(LCI_CACHE_LINE))) LCISI_cq_entry {
  enum LCIS_opcode_t op;
  int rank;
  uint32_t imm_data;
  size_t length;
  struct LCISI_endpoint_t* ep;   // ucp endpoint associated with the operation
  void* ctx;  // either LCII_packet or LCII_context passed in operations
} LCISI_cq_entry;

typedef struct __attribute__((aligned(LCI_CACHE_LINE))) LCISI_server_t {
  LCI_device_t device;
  struct LCISI_endpoint_t* endpoints[LCI_SERVER_MAX_ENDPOINTS];
  int endpoint_count;
  ucp_context_h context;
} LCISI_server_t;

typedef struct __attribute__((aligned(LCI_CACHE_LINE))) LCISI_endpoint_t {
  LCISI_server_t* server;
  ucp_worker_h worker;
  ucp_ep_h* peers;
  LCM_dequeue_t __attribute__((aligned(LCI_CACHE_LINE))) completed_ops;
  #ifdef LCI_ENABLE_MULTITHREAD_PROGRESS
  LCIU_spinlock_t cq_lock;  
  #endif
  
  LCIU_spinlock_t try_lock;
} LCISI_endpoint_t;

// pack meta (4 bytes) and rank (4 bytes) into ucp_tag_t (8 bytes)
// meta | rank
static inline ucp_tag_t pack_tag(LCIS_meta_t meta, int rank)
{
  ucp_tag_t tag;
  memcpy(&tag, &meta, sizeof(LCIS_meta_t));
  memcpy((char*)(&tag) + sizeof(LCIS_meta_t), &rank, sizeof(int));
  return tag;
}

// unpack meta and rank stored in ucp_tag_t
static inline void unpack_tag(ucp_tag_t tag, LCIS_meta_t* meta_ptr, int* int_ptr)
{
  memcpy(meta_ptr, &tag, sizeof(LCIS_meta_t));
  memcpy(int_ptr, (char*)(&tag) + sizeof(LCIS_meta_t), sizeof(int));
}

// Add a entry to completion queue
static void push_cq(void* entry)
{
  LCISI_cq_entry* cq_entry = (LCISI_cq_entry*)entry;
  LCISI_endpoint_t* ep = cq_entry->ep;
  #ifdef LCI_ENABLE_MULTITHREAD_PROGRESS  
    LCIU_acquire_spinlock(&(ep->cq_lock));
  #endif
  int status = LCM_dq_push_top(&(ep->completed_ops), entry);
  LCI_Assert(status != LCM_RETRY, "Too many entries in CQ!");
  #ifdef LCI_ENABLE_MULTITHREAD_PROGRESS  
    LCIU_release_spinlock(&(ep->cq_lock));
  #endif
}

// Struct to use when passing arguments to handler functions
typedef struct __attribute__((aligned(LCI_CACHE_LINE))) LCISI_cb_args {
  // CQ_entry associated with the operation
  LCISI_cq_entry* entry;
  // Size of the message to send/receive
  size_t size;
  // User provided buffer address
  void* buf;
  // Buffer to store packed message
  void* packed_buf;
} LCISI_cb_args;

// Called when ucp receives a message
// Unpack received data, update completion queue, free allocated buffers
static void recv_handler(void* request, ucs_status_t status,
                         const ucp_tag_recv_info_t* tag_info, void* args)
{
  LCISI_cb_args* cb_args = (LCISI_cb_args*)args;
  LCISI_cq_entry* cq_entry = cb_args->entry;
  LCISI_endpoint_t* ep = cq_entry->ep;

  // Check if user provided buffer size is enough to receive message
  LCI_Assert(cb_args->size >= tag_info->length,
             "Message size greater than allocated buffer!");
  // Check if received message length makes sense (cannot be too short)
  LCI_Assert(tag_info->length >= 0, "Message length is too short to be valid!");
  cq_entry->length = tag_info->length;

  if (cq_entry->length != 0) {
    // Nonzero message length indicates completion of recv operation
    cq_entry->op = LCII_OP_RECV;
    unpack_tag(tag_info->sender_tag, &(cq_entry->imm_data), &(cq_entry->rank));
  } else {
    // Zero message length indicates the completion of RDMA operation
    cq_entry->op = LCII_OP_RDMA_WRITE;
    unpack_tag(tag_info->sender_tag, &(cq_entry->imm_data), &(cq_entry->rank));
  }
  // Add entry to CQ
  push_cq(cq_entry);

  // Free resources
  if (cb_args->packed_buf != NULL) {
    free(cb_args->packed_buf);   
  }
  free(cb_args);
  if (request != NULL) {
    ucp_request_free(request);
  }
}

// Invoked after send is completed
// Free allocated buffer, update completion queue
static void send_handler(void* request, ucs_status_t status, void* args)
{
  LCISI_cb_args* cb_args = (LCISI_cb_args*)args;

  // Add entry to completion queue
  if (cb_args->entry != NULL) {
    push_cq(cb_args->entry);
  }

  // Free packed buffer used in ucp send
  if (cb_args->packed_buf != NULL) {
    free(cb_args->packed_buf);
  }
  if (request != NULL) {
    ucp_request_free(request);
  }
  free(cb_args);
}

// Add entry to local completion queue, send LCIS_meta and source rank to remote
// CQ
static void put_handler(void* request, ucs_status_t status, void* args)
{
  // push_cq is not invoked in this handler, as there is a chance that it is called by the worker thread
  // cq entry will be pushed in the callback of send, where LCIS_meta and rank are sent to remote
  LCISI_cb_args* cb_args = (LCISI_cb_args*)args;
  LCISI_cq_entry* cq_entry = cb_args->entry;
  LCISI_endpoint_t* ep = cq_entry->ep;
  ucp_worker_fence(ep->worker);

  // Set arguments of am send callback to free allocated resources
  LCISI_cb_args* am_cb_args = malloc(sizeof(LCISI_cb_args));
  am_cb_args->packed_buf = cb_args->packed_buf;
  am_cb_args->buf = NULL;
  am_cb_args->entry = cq_entry;

  // Send data to remote (there are pre-posted recv, so we can simply send)
  // Data to send is stored in packed_buf member of cb_args (already prepared in
  // post_put function)
  ucs_status_ptr_t put_request;
  ucp_request_param_t params;
  params.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
  // cq_entry related to put is pushed in this callback
  params.cb.send = send_handler;
  params.user_data = (void*)am_cb_args;
  put_request =
      ucp_tag_send_nbx(ep->peers[cq_entry->rank], cb_args->packed_buf, 0,
                       *((ucp_tag_t*)(cb_args->packed_buf)), &params);
  LCI_Assert(!UCS_PTR_IS_ERR(put_request),
             "Error in sending LCIS_meta during rma!");
  free(cb_args);
}

static void flush_handler(void* request, ucs_status_t status, void* args) {
  LCISI_cb_args* cb_args = (LCISI_cb_args*)args;
  LCISI_cq_entry* cq_entry = cb_args->entry;
  LCISI_endpoint_t* ep = cq_entry->ep;

  LCISI_cb_args* send_cb_args = malloc(sizeof(LCISI_cb_args));
  // buffer that stores LCIS_meta, will be freed in send callback
  send_cb_args->packed_buf = cb_args->packed_buf;
  send_cb_args->buf = NULL;
  // CQ entry created by put, pushed to local CQ in send callback
  send_cb_args->entry = cq_entry;

  ucs_status_ptr_t send_request;
  ucp_request_param_t params;
  params.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
  // cq_entry related to put is pushed in this callback
  params.cb.send = send_handler;
  params.user_data = (void*)send_cb_args;
  send_request =
      ucp_tag_send_nbx(ep->peers[cq_entry->rank], cb_args->packed_buf, 0,
                       *((ucp_tag_t*)(cb_args->packed_buf)), &params);
  LCI_Assert(!UCS_PTR_IS_ERR(send_request),
             "Error in sending LCIS_meta during rma!");
}

static void put_handler1(void* request, ucs_status_t status, void* args)
{
  // call ucp_worker_flush to ensure remote completion of put
  // within the callback of flush, call ucp_tag_send_nbx to send signal (flush_handler)
  // within the callback of send, push the CQ entry of put to local CQ (send_handler)
  LCISI_cb_args* cb_args = (LCISI_cb_args*)args;
  LCISI_cq_entry* cq_entry = cb_args->entry;
  LCISI_endpoint_t* ep = cq_entry->ep;

  // CQ entry and LCIS_meta (stored in packed_buf) of put is passed to flush callback
  // flush callback will pass the same CQ entry to send callback
  // flush callback will send packed_buf to remote using ucp_tag_send_nbx
  LCISI_cb_args* flush_cb_args = malloc(sizeof(LCISI_cb_args));
  flush_cb_args->packed_buf = cb_args->packed_buf;
  flush_cb_args->buf = NULL;
  flush_cb_args->entry = cq_entry;

  ucs_status_ptr_t flush_status;
  ucp_request_param_t flush_params;
  flush_params.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
  flush_params.cb.send = flush_handler;
  flush_params.user_data = flush_cb_args;

  flush_status = ucp_worker_flush_nbx(ep->worker, &flush_params);
  LCI_Assert(!UCS_PTR_IS_ERR(flush_status),
             "Error in flushing the put request during rma!");

  free(cb_args);
}

static void failure_handler(void* request, ucp_ep_h ep, ucs_status_t status)
{
  fprintf(stderr, "\nUCS returned the following error: %s", ucs_status_string(status));
}

static inline LCIS_mr_t LCISD_rma_reg(LCIS_server_t s, void* buf, size_t size)
{
  LCISI_server_t* server = (LCISI_server_t*)s;
  LCIS_mr_t mr;
  ucp_mem_h memh;
  ucp_mem_map_params_t params;
  ucs_status_t status;
  LCISI_memh_wrapper* wrapper = malloc(sizeof(LCISI_memh_wrapper));

  params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                      UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                      UCP_MEM_MAP_PARAM_FIELD_PROT | 
                      UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE |
                      UCP_MEM_MAP_PARAM_FIELD_FLAGS;
  params.address = buf;
  params.length = (size + 8 - 1) / 8;
  params.memory_type = UCS_MEMORY_TYPE_HOST;
  params.prot = UCP_MEM_MAP_PROT_REMOTE_WRITE | UCP_MEM_MAP_PROT_LOCAL_READ |
                UCP_MEM_MAP_PROT_LOCAL_WRITE;
  params.flags = UCP_MEM_MAP_NONBLOCK;
  // params.exported_memh_buffer = malloc(sizeof(ucp_mem_h));
  status = ucp_mem_map(server->context, &params, &memh);
  LCI_Assert(status == UCS_OK, "Error in server registration!");
  mr.address = buf;
  mr.length = size;
  wrapper->context = server->context;
  wrapper->memh = memh;
  mr.mr_p = wrapper;
  return mr;
}

static inline void LCISD_rma_dereg(LCIS_mr_t mr)
{
  ucs_status_t status;
  LCISI_memh_wrapper* wrapper = (LCISI_memh_wrapper*)mr.mr_p;
  status = ucp_mem_unmap(wrapper->context, wrapper->memh);
  if (status != UCS_OK) {
    LCI_Assert(false, "Error in server deregistration!");
  }
  free(wrapper);
}

static inline LCIS_rkey_t LCISD_rma_rkey(LCIS_mr_t mr)
{
  void* packed_addr;
  size_t packed_size;
  ucs_status_t status;
  LCISI_memh_wrapper* wrapper = (LCISI_memh_wrapper*)mr.mr_p;
  status = ucp_rkey_pack(wrapper->context, wrapper->memh, &packed_addr,
                         &packed_size);
  LCI_Assert(!UCS_PTR_IS_ERR(status), "Error in packing rkey!");
  LCI_Assert(packed_size <= sizeof(LCIS_rkey_t), "Size exceeds limit!");
  LCIS_rkey_t res;
  memset(&res, 0, sizeof(LCIS_rkey_t));
  memcpy(&res, packed_addr, packed_size);
  //ucp_rkey_buffer_release(packed_addr);
  return res;
}

//
static inline int LCISD_poll_cq(LCIS_endpoint_t endpoint_pp,
                                LCIS_cq_entry_t* entry)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  if (LCI_UCX_USE_TRY_LOCK) {
    if (LCI_UCX_PROGRESS_FOCUSED) {
      LCIU_acquire_spinlock(&(endpoint_p->try_lock));
    } else {
      if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
        return 0;
      }
    }
  }
  ucp_worker_progress(endpoint_p->worker);
  int num_entries = 0;
  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }

  #ifdef LCI_ENABLE_MULTITHREAD_PROGRESS
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->cq_lock))) return 0;
    // LCIU_acquire_spinlock(&(endpoint_p->cq_lock));
  #endif
  while (num_entries < LCI_CQ_MAX_POLL &&
         LCM_dq_size(endpoint_p->completed_ops) > 0) {
    LCISI_cq_entry* cq_entry =
        (LCISI_cq_entry*)LCM_dq_pop_bot(&(endpoint_p->completed_ops));
    entry[num_entries].ctx = cq_entry->ctx;
    entry[num_entries].imm_data = cq_entry->imm_data;
    entry[num_entries].length = cq_entry->length;
    entry[num_entries].opcode = cq_entry->op;
    entry[num_entries].rank = cq_entry->rank;
    num_entries++;
    free(cq_entry);
  }
  #ifdef LCI_ENABLE_MULTITHREAD_PROGRESS
    LCIU_release_spinlock(&(endpoint_p->cq_lock));
  #endif

  return num_entries;
}

static inline LCI_error_t LCISD_post_recv(LCIS_endpoint_t endpoint_pp,
                                          void* buf, uint32_t size,
                                          LCIS_mr_t mr, void* ctx)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  ucp_request_param_t recv_param;
  ucs_status_ptr_t request;

  // Prepare CQ entry associated with this operation
  // No need to set imm_data and rank, this is expected to arrive upon receive
  LCISI_cq_entry* cq_entry = malloc(sizeof(LCISI_cq_entry));
  cq_entry->ep = endpoint_p;
  cq_entry->length = size;
  cq_entry->op = LCII_OP_RECV;
  cq_entry->ctx = ctx;

  // Set argument for recv callback
  LCISI_cb_args* cb_args = malloc(sizeof(LCISI_cb_args));
  cb_args->entry = cq_entry;
  cb_args->buf = buf;
  cb_args->packed_buf = NULL;
  cb_args->size = size;

  // Setup recv parameters
  recv_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
      UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FLAG_NO_IMM_CMPL |
      UCP_OP_ATTR_FIELD_MEMH;
  recv_param.cb.recv = recv_handler;
  recv_param.memory_type = UCS_MEMORY_TYPE_HOST;
  recv_param.user_data = cb_args;
  recv_param.memh = ((LCISI_memh_wrapper*)mr.mr_p)->memh;

  // Receive message, check for errors
  if (LCI_UCX_USE_TRY_LOCK == true) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  request = ucp_tag_recv_nbx(endpoint_p->worker, buf, size, 0, 0,
                             &recv_param);
  LCI_Assert(!UCS_PTR_IS_ERR(request), "Error in recving message!");
  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }


  return LCI_OK;
}

static inline LCI_error_t LCISD_post_sends(LCIS_endpoint_t endpoint_pp,
                                           int rank, void* buf, size_t size,
                                           LCIS_meta_t meta)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  ucs_status_ptr_t request;

  // Prepare CQ entry associated with this operation
  LCISI_cq_entry* cq_entry = malloc(sizeof(LCISI_cq_entry));
  cq_entry->ep = endpoint_p;
  cq_entry->length = size;
  cq_entry->op = LCII_OP_SEND;
  cq_entry->rank = rank;
  cq_entry->ctx = NULL;

  // Set argument for send callback
  LCISI_cb_args* cb_args = malloc(sizeof(LCISI_cb_args));
  cb_args->entry = cq_entry;
  cb_args->packed_buf = NULL;
  cb_args->buf = NULL;

  // Setup send parameters
  ucp_request_param_t send_param;
  send_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
      UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
  send_param.cb.send = send_handler;
  send_param.user_data = cb_args;
  send_param.memory_type = UCS_MEMORY_TYPE_HOST;

  // Send message, check for errors
  // LCIS_meta_t and source rank are delievered in ucp tag
  if (LCI_UCX_USE_TRY_LOCK) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  request = ucp_tag_send_nbx(endpoint_p->peers[rank], buf, size,
                             pack_tag(meta, LCI_RANK), &send_param);
  LCI_Assert(!UCS_PTR_IS_ERR(request), "Error in posting sends!");

  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }

  return LCI_OK;
}

static inline LCI_error_t LCISD_post_send(LCIS_endpoint_t endpoint_pp, int rank,
                                          void* buf, size_t size, LCIS_mr_t mr,
                                          LCIS_meta_t meta, void* ctx)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
  ucs_status_ptr_t request;

  // Prepare CQ entry associated with this operation
  LCISI_cq_entry* cq_entry = malloc(sizeof(LCISI_cq_entry));
  cq_entry->ep = endpoint_p;
  cq_entry->length = size;
  cq_entry->op = LCII_OP_SEND;
  cq_entry->rank = rank;
  cq_entry->ctx = ctx;

  // Set argument for send callback
  LCISI_cb_args* cb_args = malloc(sizeof(LCISI_cb_args));
  cb_args->entry = cq_entry;
  cb_args->packed_buf = NULL;
  cb_args->buf = NULL;

  // Setup send parameters
  ucp_request_param_t send_param;
  send_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
      UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FIELD_MEMH |
      UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
  send_param.cb.send = send_handler;
  send_param.user_data = cb_args;
  send_param.memh = ((LCISI_memh_wrapper*)mr.mr_p)->memh;
  send_param.memory_type = UCS_MEMORY_TYPE_HOST;

  // Send message, check for errors
  if (LCI_UCX_USE_TRY_LOCK) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  request = ucp_tag_send_nbx(endpoint_p->peers[rank], buf, size,
                             pack_tag(meta, LCI_RANK), &send_param);
  LCI_Assert(!UCS_PTR_IS_ERR(request), "Error in posting send!");                  
  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }


  return LCI_OK;
}

// TODO: figure out the difference in handling messages of different sizes
static inline LCI_error_t LCISD_post_puts(LCIS_endpoint_t endpoint_pp, int rank,
                                          void* buf, size_t size,
                                          uintptr_t base, LCIS_offset_t offset,
                                          LCIS_rkey_t rkey)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;

  // Unpack the packed rkey
  ucp_rkey_h rkey_ptr;
  ucs_status_t status;
  void* tmp_rkey = malloc(sizeof(LCIS_rkey_t));
  memset(tmp_rkey, 0, sizeof(LCIS_rkey_t));
  memcpy(tmp_rkey, &rkey, sizeof(LCIS_rkey_t));
  status = ucp_ep_rkey_unpack(endpoint_p->peers[rank], tmp_rkey, &rkey_ptr);
  LCI_Assert(status == UCS_OK, "Error in unpacking RMA key!");

  // Prepare CQ entry associated with this operation
  LCISI_cq_entry* cq_entry = malloc(sizeof(LCISI_cq_entry));
  cq_entry->ep = endpoint_p;
  cq_entry->length = size;
  cq_entry->op = LCII_OP_SEND;
  cq_entry->rank = rank;
  cq_entry->ctx = NULL;

  // Set argument for send callback
  LCISI_cb_args* cb_args = malloc(sizeof(LCISI_cb_args));
  cb_args->entry = cq_entry;
  cb_args->buf = NULL;
  cb_args->packed_buf = NULL;

  // Setup send parameters
  ucp_request_param_t put_param;
  put_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
      UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
  // No need to signal remote completion
  put_param.cb.send = send_handler;
  put_param.user_data = cb_args;
  put_param.memory_type = UCS_MEMORY_TYPE_HOST;

  // Send message, check for errors
  uint64_t remote_addr = (uint64_t)((char*)base + offset);
  ucs_status_ptr_t request;
  if (LCI_UCX_USE_TRY_LOCK) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  request = ucp_put_nbx(endpoint_p->peers[rank], buf, size, remote_addr,
                        rkey_ptr, &put_param);
  LCI_Assert(!UCS_PTR_IS_ERR(request), "Error in RMA puts operation!");

  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }

  return LCI_OK;
}

static inline LCI_error_t LCISD_post_put(LCIS_endpoint_t endpoint_pp, int rank,
                                         void* buf, size_t size, LCIS_mr_t mr,
                                         uintptr_t base, LCIS_offset_t offset,
                                         LCIS_rkey_t rkey, void* ctx)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;

  // Unpack the packed rkey
  ucp_rkey_h rkey_ptr;
  ucs_status_t status;
  void* tmp_rkey = malloc(sizeof(LCIS_rkey_t));
  memset(tmp_rkey, 0, sizeof(LCIS_rkey_t));
  memcpy(tmp_rkey, &rkey, sizeof(LCIS_rkey_t));
  status = ucp_ep_rkey_unpack(endpoint_p->peers[rank], tmp_rkey, &rkey_ptr);
  LCI_Assert(status == UCS_OK, "Error in unpacking RMA key!");

  // Prepare CQ entry associated with this operation
  LCISI_cq_entry* cq_entry = malloc(sizeof(LCISI_cq_entry));
  cq_entry->ep = endpoint_p;
  cq_entry->length = size;
  cq_entry->op = LCII_OP_SEND;
  cq_entry->rank = rank;
  cq_entry->ctx = ctx;

  // Set argument for send callback
  LCISI_cb_args* cb_args = malloc(sizeof(LCISI_cb_args));
  cb_args->entry = cq_entry;
  cb_args->buf = NULL;
  cb_args->packed_buf = NULL;

  #ifndef LCI_UCX_USE_SEGMENTED_PUT
  // Setup send parameters
  ucp_request_param_t put_param;
  put_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
      UCP_OP_ATTR_FIELD_USER_DATA;
  // No need to signal remote completion
  put_param.cb.send = send_handler;
  put_param.user_data = cb_args;
  put_param.memory_type = UCS_MEMORY_TYPE_HOST;

  // Send message, check for errors
  uint64_t remote_addr = (uint64_t)((char*)base + offset);
  ucs_status_ptr_t request;
  if (LCI_UCX_USE_TRY_LOCK) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  request = ucp_put_nbx(endpoint_p->peers[rank], buf, size, remote_addr,
                        rkey_ptr, &put_param);
  LCI_Assert(!UCS_PTR_IS_ERR(request), "Error in RMA puts operation!");
  if (request == NULL) {
    ucs_status_t unused;
    send_handler(NULL, unused, cb_args);
  }
  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }
  #else
  if (LCI_UCX_USE_TRY_LOCK) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  uint64_t remote_addr = base + offset;
  int num_segments = (size + 65536 - 1) / (65536 - 1);
  for (int i = 0; i < num_segments; i++) {
    ucs_status_ptr_t request;
    if (i == num_segments - 1) {
      // put message in chunks with size 65536
      // only use callback to signal completion in the last put
      // Use ucp_worker_fence to ensure that this is the last completed operation
      ucp_worker_fence(endpoint_p->worker);
      ucp_request_param_t put_param;
      put_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
                               UCP_OP_ATTR_FIELD_USER_DATA;
      // No need to signal remote completion
      put_param.cb.send = send_handler;
      put_param.user_data = cb_args;
      put_param.memory_type = UCS_MEMORY_TYPE_HOST;
      request = ucp_put_nbx(endpoint_p->peers[rank], buf + i * 65536, size - i * 65536, remote_addr + i * 65536,
                            rkey_ptr, &put_param);
    } else {
      // do not signal remote completion in callback
      // send_handler is just used to free resources
      ucp_request_param_t put_param;
      put_param.op_attr_mask = UCP_OP_ATTR_FIELD_MEMORY_TYPE;
      put_param.cb.send = send_handler;
      put_param.user_data = cb_args;
      put_param.memory_type = UCS_MEMORY_TYPE_HOST;
      request = ucp_put_nbx(endpoint_p->peers[rank], buf + i * 65536, 65536, remote_addr + i * 65536,
                            rkey_ptr, &put_param);
    }
  }
  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }
  #endif

  return LCI_OK;
}

// Put and send meta to remote CQ
static inline LCI_error_t LCISD_post_putImms(LCIS_endpoint_t endpoint_pp,
                                             int rank, void* buf, size_t size,
                                             uintptr_t base,
                                             LCIS_offset_t offset,
                                             LCIS_rkey_t rkey, uint32_t meta)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;

  // Unpack the packed rkey
  ucp_rkey_h rkey_ptr;
  ucs_status_t status;
  void* tmp_rkey = malloc(sizeof(LCIS_rkey_t));
  memset(tmp_rkey, 0, sizeof(LCIS_rkey_t));
  memcpy(tmp_rkey, &rkey, sizeof(LCIS_rkey_t));
  status = ucp_ep_rkey_unpack(endpoint_p->peers[rank], tmp_rkey, &rkey_ptr);
  LCI_Assert(status == UCS_OK, "Error in unpacking RMA key!");

  // Prepare CQ entry associated with this operation
  LCISI_cq_entry* cq_entry = malloc(sizeof(LCISI_cq_entry));
  cq_entry->ep = endpoint_p;
  cq_entry->length = size;
  cq_entry->op = LCII_OP_SEND;
  cq_entry->rank = rank;
  cq_entry->ctx = NULL;

  // Set argument for send callback
  LCISI_cb_args* cb_args = malloc(sizeof(LCISI_cb_args));
  // Stores LCIS_meta and source rank to send to remote completion queue
  ucp_tag_t* packed_buf = malloc(sizeof(ucp_tag_t));
  *packed_buf = pack_tag(meta, LCI_RANK);

  cb_args->entry = cq_entry;
  cb_args->buf = NULL;
  cb_args->packed_buf = packed_buf;

  // Setup send parameters
  ucp_request_param_t put_param;
  put_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
      UCP_OP_ATTR_FIELD_USER_DATA;
  // Deliever data to remote CQ with active message
  put_param.cb.send = put_handler1;
  put_param.user_data = cb_args;
  put_param.memory_type = UCS_MEMORY_TYPE_HOST;
  
  #ifndef LCI_UCX_USE_SEGMENTED_PUT
  // Send message, check for errors
  uint64_t remote_addr = (uint64_t)((char*)base + offset);
  ucs_status_ptr_t request;
  if (LCI_UCX_USE_TRY_LOCK) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  request = ucp_put_nbx(endpoint_p->peers[rank], buf, size, remote_addr,
                        rkey_ptr, &put_param);
  LCI_Assert(!UCS_PTR_IS_ERR(request), "Error in RMA puts operation!");  
  if (request == NULL) {
    ucs_status_t unused;
    put_handler1(NULL, unused, cb_args);
  }                        
  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  } 
  #else
  uint64_t remote_addr = base + offset;
  if (LCI_UCX_USE_TRY_LOCK) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  int num_segments = (size + 65536 - 1) / (65536 - 1);
  for (int i = 0; i < num_segments; i++) {
    ucs_status_ptr_t request;
    if (i == num_segments - 1) {
      // put message in chunks with size 65536
      // only use callback to signal completion in the last put
      // Use ucp_worker_fence to ensure that this is the last completed operation
      ucp_worker_fence(endpoint_p->worker);
      ucp_request_param_t put_param;
      put_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
                               UCP_OP_ATTR_FIELD_USER_DATA;
      put_param.cb.send = put_handler;
      put_param.user_data = cb_args;
      put_param.memory_type = UCS_MEMORY_TYPE_HOST;
      request = ucp_put_nbx(endpoint_p->peers[rank], buf + i * 65536, size - i * 65536, remote_addr + i * 65536,
                            rkey_ptr, &put_param);
    } else {
      // do not signal remote completion in callback
      // send_handler is just used to free resources
      ucp_request_param_t put_param;
      put_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
                               UCP_OP_ATTR_FIELD_USER_DATA;                         
      put_param.cb.send = send_handler;
      put_param.user_data = cb_args;
      put_param.memory_type = UCS_MEMORY_TYPE_HOST;
      request = ucp_put_nbx(endpoint_p->peers[rank], buf + i * 65536, 65536, remote_addr + i * 65536,
                            rkey_ptr, &put_param);
    }
  }
  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }
  #endif

  return LCI_OK;
}

static inline LCI_error_t LCISD_post_putImm(LCIS_endpoint_t endpoint_pp,
                                            int rank, void* buf, size_t size,
                                            LCIS_mr_t mr, uintptr_t base,
                                            LCIS_offset_t offset,
                                            LCIS_rkey_t rkey, LCIS_meta_t meta,
                                            void* ctx)
{
  LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;

  // Unpack the packed rkey
  ucp_rkey_h rkey_ptr;
  ucs_status_t status;
  void* tmp_rkey = malloc(sizeof(LCIS_rkey_t));
  memset(tmp_rkey, 0, sizeof(LCIS_rkey_t));
  memcpy(tmp_rkey, &rkey, sizeof(LCIS_rkey_t));
  status = ucp_ep_rkey_unpack(endpoint_p->peers[rank], tmp_rkey, &rkey_ptr);
  LCI_Assert(status == UCS_OK, "Error in unpacking RMA key!");

  // Prepare CQ entry associated with this operation
  LCISI_cq_entry* cq_entry = malloc(sizeof(LCISI_cq_entry));
  cq_entry->ep = endpoint_p;
  cq_entry->length = size;
  cq_entry->op = LCII_OP_SEND;
  cq_entry->rank = rank;
  cq_entry->ctx = ctx;

  // Set argument for send callback
  LCISI_cb_args* cb_args = malloc(sizeof(LCISI_cb_args));
  // Stores LCIS_meta and source rank to send to remote completion queue
  ucp_tag_t* packed_buf = malloc(sizeof(ucp_tag_t));
  *(packed_buf) = pack_tag(meta, LCI_RANK);

  cb_args->entry = cq_entry;
  cb_args->buf = endpoint_p->peers[rank];
  cb_args->packed_buf = packed_buf;

  // Setup send parameters
  ucp_request_param_t put_param;
  put_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
      UCP_OP_ATTR_FIELD_USER_DATA;
  // Deliever data to remote CQ with active message
  put_param.cb.send = put_handler1;
  put_param.user_data = cb_args;
  put_param.memory_type = UCS_MEMORY_TYPE_HOST;
  #ifndef LCI_UCX_USE_SEGMENTED_PUT
  // Send message, check for errors
  uint64_t remote_addr = (uint64_t)((char*)base + offset);
  ucs_status_ptr_t request;
  if (LCI_UCX_USE_TRY_LOCK) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  request = ucp_put_nbx(endpoint_p->peers[rank], buf, size, remote_addr,
                        rkey_ptr, &put_param);
  LCI_Assert(!UCS_PTR_IS_ERR(request), "Error in RMA put operation!");
  if (request == NULL) {
    ucs_status_t unused;
    put_handler1(NULL, unused, cb_args);
  }                        
  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }
  #else
  if (LCI_UCX_USE_TRY_LOCK) {
    if (!LCIU_try_acquire_spinlock(&(endpoint_p->try_lock))) {
      return LCI_ERR_RETRY_LOCK;
    }
  }
  uint64_t remote_addr = base + offset;
  int num_segments = (size + 65536 - 1) / (65536 - 1);
  for (int i = 0; i < num_segments; i++) {
    ucs_status_ptr_t request;
    if (i == num_segments - 1) {
      // put message in chunks with size 65536
      // only use callback to signal completion in the last put
      // Use ucp_worker_fence to ensure that this is the last completed operation
      ucp_worker_fence(endpoint_p->worker);
      ucp_request_param_t put_param;
      put_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
                               UCP_OP_ATTR_FIELD_USER_DATA;
      put_param.cb.send = put_handler;
      put_param.user_data = cb_args;
      put_param.memory_type = UCS_MEMORY_TYPE_HOST;
      request = ucp_put_nbx(endpoint_p->peers[rank], buf + i * 65536, size - i * 65536, remote_addr + i * 65536,
                            rkey_ptr, &put_param);
    } else {
      // do not signal remote completion in callback
      // send_handler is just used to free resources
      ucp_request_param_t put_param;
      put_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE |
                               UCP_OP_ATTR_FIELD_USER_DATA;                         
      put_param.cb.send = send_handler;
      put_param.user_data = cb_args;
      put_param.memory_type = UCS_MEMORY_TYPE_HOST;
      request = ucp_put_nbx(endpoint_p->peers[rank], buf + i * 65536, 65536, remote_addr + i * 65536,
                            rkey_ptr, &put_param);
    }
  }
  if (LCI_UCX_USE_TRY_LOCK) {
    LCIU_release_spinlock(&(endpoint_p->try_lock));
  }
  #endif


  return LCI_OK;
}

#endif
