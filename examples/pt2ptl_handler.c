#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct {
  int peer_rank;
  LCI_tag_t tag;
  LCI_lbuffer_t src_buf;
  LCI_lbuffer_t dst_buf;
  bool flag;
} Args_t;

// Our function handler to invoke after a send completes
void send_handler(LCI_request_t request)
{
  // Once the associated operation completes, the active message handler will be
  // invoked and the completion information will be written to the request
  // object.
  Args_t* args = request.user_context;
  assert(request.flag == LCI_OK);
  assert(request.rank == args->peer_rank);
  assert(request.tag == args->tag);
  assert(request.type == LCI_LONG);
  assert(request.data.lbuffer.address == args->src_buf.address);
  assert(request.data.lbuffer.length == args->src_buf.length);
  assert(request.data.lbuffer.segment == args->src_buf.segment);
  args->flag = true;
}

// Our function handler to invoke after a receive completes
void recv_handler(LCI_request_t request)
{
  // Once the associated operation completes, the active message handler will be
  // invoked and the completion information will be written to the request
  // object.
  Args_t* args = request.user_context;
  assert(request.flag == LCI_OK);
  assert(request.rank == args->peer_rank);
  assert(request.tag == args->tag);
  assert(request.type == LCI_LONG);
  assert(request.data.lbuffer.address == args->dst_buf.address);
  assert(request.data.lbuffer.length == args->dst_buf.length);
  assert(request.data.lbuffer.segment == args->dst_buf.segment);
  for (int j = 0; j < args->dst_buf.length; ++j) {
    assert(((char*)args->dst_buf.address)[j] == 'a' + args->peer_rank);
  }
  args->flag = true;
}

int main(int argc, char** args)
{
  // Number of messages to send
  int num_msgs = 10;
  int msgs_size = 65536;
  if (argc > 1) num_msgs = atoi(args[1]);
  if (argc > 2) msgs_size = atoi(args[1]);
  // Call `LCI_initialize` to initialize the runtime
  LCI_initialize();
  // Initialize a device. A LCI device is associated with a set of communication
  // resources (matching table, low-level network resources, etc).
  // Alternatively, users can use LCI_UR_DEVICE, which
  // has been initialized by the runtime in LCI_initialize().
  LCI_device_t device;
  LCI_device_init(&device);
  // Initialize an endpoint. Alternatively, users can use LCI_UR_ENDPOINT,
  // which has been initialized by the runtime in LCI_initialize().
  LCI_endpoint_t ep;
  // We need a property list to initialize the endpoint. Multiple properties of
  // the endpoint can be specified by the property list.
  // Currently, a LCI endpoint is not associated with any low-level
  // communication resources, it is just a way to specify a bunch of
  // configurations.
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  // We set the completion mechanism on the sender side to be active message
  // handler.
  LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, LCI_COMPLETION_HANDLER);
  // We set the completion mechanism on the receiver side to be active message
  // handler.
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_HANDLER);
  // We set the matching rule to be rank+tag, so a pair of LCI send and recv
  // will be matched through (source rank, destination rank, tag).
  // Alternatively, you can set the matching rule to tag-only by LCI_MATCH_TAG.
  LCI_plist_set_match_type(plist, LCI_MATCH_RANKTAG);
  LCI_endpoint_init(&ep, device, plist);
  LCI_plist_free(&plist);

  // Create two completion objects referring to our function handlers
  LCI_comp_t send_comp, recv_comp;
  LCI_handler_create(device, send_handler, &send_comp);
  LCI_handler_create(device, recv_handler, &recv_comp);

  // For sendl and recvl, we need to allocate our own send/recv buffers.
  LCI_lbuffer_t src_buf, dst_buf;
  src_buf.address = malloc(msgs_size);
  src_buf.length = msgs_size;
  memset(src_buf.address, 'a' + LCI_RANK, msgs_size);
  dst_buf.address = malloc(msgs_size);
  dst_buf.length = msgs_size;
  memset(dst_buf.address, 0, msgs_size);

  // For long messages, we can use LCI_sendl and LCI_recvl to send and receive
  // messages. Users need to explicitly register/deregister the send and
  // receive buffers.
  LCI_memory_register(device, src_buf.address, src_buf.length,
                      &src_buf.segment);
  LCI_memory_register(device, dst_buf.address, dst_buf.length,
                      &dst_buf.segment);
  // Alternatively, users can pass a LCI_SEGMENT_ALL and the LCI runtime will
  // register and deregister them for users.
  // src_buf.segment = LCI_SEGMENT_ALL;
  // dst_buf.segment = LCI_SEGMENT_ALL;

  // Alternatively, users can directly use LCI_lbuffer_alloc to allocate and
  // register the buffers.
  // LCI_lbuffer_alloc(device, msgs_size, &src_buf);
  // LCI_lbuffer_alloc(device, msgs_size, &dst_buf);

  int peer_rank;
  if (LCI_NUM_PROCESSES == 1) {
    // We will do loopback messages
    peer_rank = 0;
  } else if (LCI_NUM_PROCESSES == 2) {
    // We will do a simple ping-pong here between rank 0 and rank 1.
    // The destination rank to send and recv messages.
    peer_rank = 1 - LCI_RANK;
  } else {
    fprintf(stderr, "Unexpected process number!");
  }
  // The tag of the messages. Send and recv will be matched based on the tag
  // or rank+tag, depending on the matching rule. You can set the match rule by
  // LCI_plist_set_match_type.
  LCI_tag_t tag = 99;

  // The arguments we want to pass to the handlers.
  Args_t send_args, recv_args;
  send_args.peer_rank = peer_rank;
  send_args.tag = tag;
  send_args.src_buf = src_buf;
  send_args.dst_buf = dst_buf;
  send_args.flag = false;
  recv_args = send_args;

  if (LCI_RANK == 0) {
    for (int i = 0; i < num_msgs; i++) {
      // Recv a long message using LCI_recvl
      // Here we have to post a receive before waiting for the send to complete
      // to make the loopback case (LCI_NUM_PROCESSES==1) to work, as the send
      // will not complete before the corresponding receive is posted for long
      // messages.
      recv_args.flag = false;
      LCI_recvl(ep, dst_buf, peer_rank, tag, recv_comp, &recv_args);

      // Send a long message using LCI_sendl
      // A LCI send function can return LCI_ERR_RETRY, so we use a while loop
      // here to make sure the message is sent
      send_args.flag = false;
      while (LCI_sendl(ep, src_buf, peer_rank, tag, send_comp, &send_args) ==
             LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      // Wait for the send to complete.
      while (send_args.flag == false) {
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      }
      // Wait for the receive to complete.
      while (recv_args.flag == false)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
    }
  } else {
    for (int i = 0; i < num_msgs; i++) {
      recv_args.flag = false;
      LCI_recvl(ep, dst_buf, peer_rank, tag, recv_comp, &recv_args);
      while (recv_args.flag == false) LCI_progress(device);

      send_args.flag = false;
      while (LCI_sendl(ep, src_buf, peer_rank, tag, send_comp, &send_args) ==
             LCI_ERR_RETRY)
        LCI_progress(device);
      while (send_args.flag == false) {
        LCI_progress(device);
      }
    }
  }
  // Free all the resources
  // If you are using LCI_lbuffer_alloc
  // LCI_lbuffer_free(src_buf);
  // LCI_lbuffer_free(dst_buf);
  // If you are using LCI_SEGMENT_ALL
  // assert(src_buf.segment == LCI_SEGMENT_ALL);
  // assert(dst_buf.segment == LCI_SEGMENT_ALL);
  LCI_memory_deregister(&src_buf.segment);
  LCI_memory_deregister(&dst_buf.segment);
  free(src_buf.address);
  free(dst_buf.address);
  LCI_endpoint_free(&ep);
  LCI_device_free(&device);
  // Call `LCI_finalize` to finalize the runtime
  LCI_finalize();
  return 0;
}
