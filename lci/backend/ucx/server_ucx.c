#include "runtime/lcii.h"
#include "backend/ucx/server_ucx.h"

#define ENCODED_LIMIT 8192 // length of buffer to store encoded ucp address during initialization, user can change it
#define DECODED_LIMIT 8192

static int g_endpoint_num = 0;

// Encodes a ucp address into its hex representation as a string
// my_addrs should have null bytes
// encoded_value buffer should have enough size to store encoded content
// addrs_length is necessary as an input since original address has nulls inside
// it
void encode_ucp_address(char* my_addrs, int addrs_length, char* encoded_value)
{
  // Encoding as hexdecimal at most doubles the length, so available length should be at least twice
  // of the original length to avoid overflow
  LCI_Assert(ENCODED_LIMIT >= 2 * addrs_length, "Buffer to store encoded address is too short! Use a higher ENCODED_LIMIT");
  int segs = (addrs_length + sizeof(uint64_t) - 1) / sizeof(uint64_t);
  for (int i = 0; i < segs; i++) {
    sprintf(encoded_value + 2 * i * sizeof(uint64_t), "%016lx",
            ((uint64_t*)my_addrs)[i]);
  }
}

// decoded_addrs should be initialized to 0 and have sufficient size
// no need to provide length as encoded_addrs is one single string
void decode_ucp_address(char* encoded_addrs, char* decoded_addrs)
{
  // Avoid overflow
  LCI_Assert(DECODED_LIMIT >= strlen(encoded_addrs), "Buffer to store decoded address is too short! Use a higher DECODED_LIMIT");
  int segs = (strlen(encoded_addrs) + 16 - 1) / 16;
  char tmp_buf[17];
  tmp_buf[16] = 0;
  for (int i = 0; i < segs; i++) {
    memcpy(tmp_buf, encoded_addrs + i * 16, 16);
    *((uint64_t*)(decoded_addrs + i * sizeof(uint64_t))) =
        strtoul(tmp_buf, NULL, 16);
  }
}

// Publish an encoded address
// Splits into segment if address length exceeds PMI string limit
// Keys are in the format of "LCI_ENC_ep_rank_segment"
void publish_address(char* encoded_addrs, int endpoint_id, size_t* num_segments)
{
  size_t length = strlen(encoded_addrs);
  *num_segments =
      (length + LCT_PMI_STRING_LIMIT - 2) / (LCT_PMI_STRING_LIMIT - 1);
  // store 254 bytes of actual data and 1 byte of terminator (null)
  for (int i = 0; i < *num_segments; i++) {
    char seg[LCT_PMI_STRING_LIMIT];
    char seg_key[LCT_PMI_STRING_LIMIT];
    memset(seg, 0, LCT_PMI_STRING_LIMIT);
    memset(seg_key, 0, LCT_PMI_STRING_LIMIT);
    if (i == *num_segments - 1) {
      memcpy(seg, encoded_addrs + i * (LCT_PMI_STRING_LIMIT - 1),
             length - i * (LCT_PMI_STRING_LIMIT - 1));
    } else {
      memcpy(seg, encoded_addrs + i * (LCT_PMI_STRING_LIMIT - 1),
             LCT_PMI_STRING_LIMIT - 1);
    }
    sprintf(seg_key, "LCI_ENC_%d_%d_%d", endpoint_id, LCI_RANK, i);
    LCT_pmi_publish(seg_key, seg);
  }
}

// Retrieves segmented encoded address into one long encoded address
// combined_addrs should have sufficient size and initialized to 0
void get_address(size_t num_segments, int endpoint_id, int rank,
                 char* combined_addrs)
{
  for (int i = 0; i < num_segments; i++) {
    char seg[LCT_PMI_STRING_LIMIT];
    char seg_key[LCT_PMI_STRING_LIMIT];
    memset(seg, 0, LCT_PMI_STRING_LIMIT);
    memset(seg_key, 0, LCT_PMI_STRING_LIMIT);
    sprintf(seg_key, "LCI_ENC_%d_%d_%d", endpoint_id, rank, i);
    LCT_pmi_getname(rank, seg_key, seg);
    memcpy(combined_addrs + i * (LCT_PMI_STRING_LIMIT - 1), seg,
           LCT_PMI_STRING_LIMIT - 1);
  }
}

void LCISD_server_init(LCI_device_t device, LCIS_server_t* s)
{
  LCISI_server_t* server = LCIU_malloc(sizeof(LCISI_server_t));
  *s = (LCIS_server_t)server;
  server->device = device;

  // Create server (ucp_context)
  ucs_status_t status;
  ucp_config_t* config;
  status = ucp_config_read(NULL, NULL, &config);
  ucp_params_t params;
  params.field_mask = UCP_PARAM_FIELD_FEATURES;
  params.features = UCP_FEATURE_TAG | UCP_FEATURE_RMA | UCP_FEATURE_AM;
  ucp_context_h context;
  status = ucp_init(&params, config, &context);
  server->context = context;
  server->endpoint_count = 0;
}

// Currently empty, otherwise uncompleted request (by preposting receive) will
// result in errors
void LCISD_server_fina(LCIS_server_t s)
{
  //   LCISI_server_t* server = (LCISI_server_t*)s;
  //   LCI_Assert(server->endpoint_count == 0, "Endpoint count is not zero
  //   (%d)\n",
  //              server->endpoint_count);
  //   ucp_cleanup(server->context);
  //   free(s);
}

void LCISD_endpoint_init(LCIS_server_t server_pp, LCIS_endpoint_t* endpoint_pp,
                         bool single_threaded)
{
  int endpoint_id = g_endpoint_num++;
  LCISI_endpoint_t* endpoint_p = LCIU_malloc(sizeof(LCISI_endpoint_t));
  *endpoint_pp = (LCIS_endpoint_t)endpoint_p;
  endpoint_p->server = (LCISI_server_t*)server_pp;
  endpoint_p->server->endpoints[endpoint_p->server->endpoint_count++] =
      endpoint_p;

  // Create endpoint (ucp_worker)
  ucp_worker_h worker;
  ucp_worker_params_t params;
  ucs_status_t status;
  params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE | UCP_WORKER_PARAM_FIELD_FLAGS;
  params.flags = UCP_WORKER_FLAG_IGNORE_REQUEST_LEAK;
  if (single_threaded) {
    params.thread_mode = UCS_THREAD_MODE_SINGLE;
  } else {
    params.thread_mode = UCS_THREAD_MODE_MULTI;
  }

  status = ucp_worker_create(endpoint_p->server->context, &params, &worker);
  LCI_Assert(status == UCS_OK, "Error in creating UCP worker!");
  endpoint_p->worker = worker;

// Create lock
  #ifdef LCI_ENABLE_MULTITHREAD_PROGRESS
    LCIU_spinlock_init(&(endpoint_p->cq_lock));
    printf("\nUsing multiple progress threads");
  #endif
  if (LCI_UCX_USE_TRY_LOCK == true) {
    LCIU_spinlock_init(&(endpoint_p->try_lock));
    printf("\nUsing try lock for progress and send/recv");
    if (LCI_UCX_PROGRESS_FOCUSED) printf("\nGiving priority to lock for progress thread");
  }
  // Create completion queue
  LCM_dq_init(&endpoint_p->completed_ops, 2 * LCI_PACKET_SIZE);

  // Exchange endpoint address
  endpoint_p->peers = LCIU_malloc(sizeof(ucp_ep_h) * LCI_NUM_PROCESSES);
  ucp_address_t* my_addrs;
  size_t addrs_length;
  status = ucp_worker_get_address(worker, &my_addrs, &addrs_length);
  LCI_Assert(status == UCS_OK, "Error in getting worker address!");

  // Publish worker address
  // Worker address is encoded into a string of hex representation of original
  // address Keys to use when publishing address (number of segments encoded
  // address is divided into)
  char seg_key[LCT_PMI_STRING_LIMIT + 1];
  memset(seg_key, 0, LCT_PMI_STRING_LIMIT + 1);

  // Buffers to store published contents
  char encoded_value[ENCODED_LIMIT];
  char seg_value[sizeof(size_t) + 1];
  memset(encoded_value, 0, ENCODED_LIMIT);
  memset(seg_value, 0, sizeof(size_t) + 1);

  // Set key
  sprintf(seg_key, "LCI_SEG_%d_%d", endpoint_id, LCI_RANK);

  // Encode the address
  encode_ucp_address((char*)my_addrs, addrs_length, encoded_value);

  // Publish address, get number of segments
  size_t num_segments;
  publish_address(encoded_value, endpoint_id, &num_segments);

  // Publish number of segments that the encoded addrs is divided into
  memcpy(seg_value, &num_segments, sizeof(size_t));
  LCT_pmi_publish(seg_key, seg_value);

  LCT_pmi_barrier();

  // Receive peer address
  // Buffer to store decoded address
  char decoded_value[DECODED_LIMIT];
  memset(decoded_value, 0, DECODED_LIMIT);

  for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
    size_t size;
    // Create ucp endpoint to connect workers
    ucp_ep_params_t ep_params;
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS |
                           UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
                           UCP_EP_PARAM_FIELD_ERR_HANDLER |
                           UCP_EP_PARAM_FIELD_USER_DATA;
    ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
    ep_params.err_handler.cb = failure_handler;
    ep_params.err_handler.arg = NULL;
    ep_params.user_data = NULL;
    // Receive information (address) required to create ucp endpoint
    if (i != LCI_RANK) {
      // Reset keys
      memset(seg_key, 0, LCT_PMI_STRING_LIMIT + 1);

      // Reset values
      memset(encoded_value, 0, ENCODED_LIMIT);
      memset(seg_value, 0, sizeof(size_t) + 1);

      // Set correct keys
      sprintf(seg_key, "LCI_SEG_%d_%d", endpoint_id, i);

      // Get number of segments
      LCT_pmi_getname(i, seg_key, seg_value);

      // Combine segmented address
      get_address(*((size_t*)seg_value), endpoint_id, i, encoded_value);

      // Reset buffer, decode address
      memset(decoded_value, 0, DECODED_LIMIT);
      decode_ucp_address(encoded_value, decoded_value);

      // Set peer address
      ep_params.address = (ucp_address_t*)decoded_value;
    } else {
      ep_params.address = my_addrs;
    }
    ucp_ep_h peer;
    ucs_status_t status1;
    status1 = ucp_ep_create(worker, &ep_params, &peer);
    LCI_Assert(status1 == UCS_OK, "Error in creating peer endpoints!");
    (endpoint_p->peers)[i] = peer;
  }
  LCT_pmi_barrier();
}

// Currently empty, otherwise uncompleted request (by preposting receive) will
// result in errors
void LCISD_endpoint_fina(LCIS_endpoint_t endpoint_pp)
{
    LCT_pmi_barrier();
    LCISI_endpoint_t* endpoint_p = (LCISI_endpoint_t*)endpoint_pp;
    int my_idx = --endpoint_p->server->endpoint_count;
    LCI_Assert(endpoint_p->server->endpoints[my_idx] == endpoint_p,
               "This is not me!\n");
    endpoint_p->server->endpoints[my_idx] = NULL;
    for (int i = 0; i < LCI_NUM_PROCESSES; i++) {
      ucs_status_ptr_t status;
      ucp_request_param_t params;
      params.flags = UCP_EP_CLOSE_FLAG_FORCE;
      status = ucp_ep_close_nbx((endpoint_p->peers)[i], &params);
    }

    // Should other ucp ep owned by other workers be destoryed?
    ucp_worker_destroy(endpoint_p->worker);
    LCM_dq_finalize(&(endpoint_p->completed_ops));
    free(endpoint_pp);
}
