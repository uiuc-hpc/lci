#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

LCI_request_t request_g;

// Our function handler to invoke after a receive completes
void recv_handler(LCI_request_t request)
{
  // Once the associated operation completes, the active message handler will be
  // invoked and the completion information will be written to the request
  // object.
  request_g = request;
}

int main(int argc, char** args)
{
  // Number of messages to send
  int num_msgs = 10;
  int msgs_size = 8192;
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

  // Create one completion object referring to our function handler.
  LCI_comp_t recv_comp;
  LCI_handler_create(device, recv_handler, &recv_comp);

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
  // Completion mechanism on the sender side does not matter.
  // LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, LCI_COMPLETION_HANDLER);
  // We set the completion mechanism on the receiver side to be active message
  // handler.
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_HANDLER);
  // Set the default completion object to be triggered by
  // LCI_DEFAULT_COMP_REMOTE.
  LCI_plist_set_default_comp(plist, recv_comp);
  // Matching rule does not matter here any more, as we are not using send/recv
  // LCI_plist_set_match_type(plist, LCI_MATCH_RANKTAG);
  LCI_endpoint_init(&ep, device, plist);
  LCI_plist_free(&plist);

  // For short messages (up to LCI_SHORT_SIZE bytes), we can use LCI_puts to
  // send messages.
  LCI_short_t message;
  // Suppose we want to send an uint64_t.
  if (sizeof(uint64_t) > LCI_SHORT_SIZE) {
    fprintf(stderr,
            "The message is too long to be sent"
            "using LCI_puts");
    exit(1);
  }
  *(uint64_t*)&message = 9527 + LCI_RANK;

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
  // The tag of the messages. Since we are using the one-sided put, no tag
  // matching will happen. This tag will just be passed to the receive side as
  // a value.
  LCI_tag_t tag = 99 + LCI_RANK;
  LCI_tag_t peer_tag = 99 + peer_rank;
  request_g.flag = LCI_ERR_RETRY;
  if (LCI_RANK == 0) {
    for (int i = 0; i < num_msgs; i++) {
      request_g.flag = LCI_ERR_RETRY;
      // Send a short message using LCI_sends.
      // A LCI send function can return LCI_ERR_RETRY, so we use a while loop
      // here to make sure the message is sent.
      while (LCI_puts(ep, message, peer_rank, tag, LCI_DEFAULT_COMP_REMOTE) ==
             LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);

      // Wait for the receive to complete.
      while (request_g.flag == LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      // The active message
      assert(request_g.flag == LCI_OK);
      assert(request_g.rank == peer_rank);
      assert(request_g.tag == peer_tag);
      assert(request_g.type == LCI_IMMEDIATE);
      assert(*(uint64_t*)&request_g.data.immediate == 9527 + peer_rank);
      // request_g.user_context is undefined.
      request_g.flag = LCI_ERR_RETRY;
    }
  } else {
    for (int i = 0; i < num_msgs; i++) {
      while (request_g.flag == LCI_ERR_RETRY) LCI_progress(device);
      assert(request_g.flag == LCI_OK);
      assert(request_g.rank == peer_rank);
      assert(request_g.tag == peer_tag);
      assert(request_g.type == LCI_IMMEDIATE);
      assert(*(uint64_t*)&request_g.data.immediate == 9527 + peer_rank);
      request_g.flag = LCI_ERR_RETRY;

      while (LCI_puts(ep, message, peer_rank, tag, LCI_DEFAULT_COMP_REMOTE) ==
             LCI_ERR_RETRY)
        LCI_progress(device);
    }
  }
  // Free all the resources
  LCI_endpoint_free(&ep);
  LCI_device_free(&device);
  // Call `LCI_finalize` to finalize the runtime
  LCI_finalize();
  return 0;
}
