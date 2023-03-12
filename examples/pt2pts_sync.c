#include "lci.h"
#include <stdio.h>
#include <assert.h>

int main(int argc, char** args)
{
  // Number of messages to send
  int num_msgs = 10;
  if (argc > 1) num_msgs = atoi(args[1]);
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
  // We set the completion mechanism on the sender side to be synchronizer.
  LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, LCI_COMPLETION_SYNC);
  // We set the completion mechanism on the receiver side to be synchronizer.
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC);
  // We set the matching rule to be rank+tag, so a pair of LCI send and recv
  // will be matched through (source rank, destination rank, tag).
  // Alternatively, you can set the matching rule to tag-only by LCI_MATCH_TAG.
  LCI_plist_set_match_type(plist, LCI_MATCH_RANKTAG);
  LCI_endpoint_init(&ep, device, plist);
  LCI_plist_free(&plist);

  // Create a synchronizer. You can treat it as an MPI_Request for now.
  // Setting the threshold to 1 means it will be triggered by the completion
  // of one asynchronous operation.
  LCI_comp_t sync;
  LCI_sync_create(device, 1, &sync);

  // For short messages (up to LCI_SHORT_SIZE bytes), we can use LCI_sends
  // and LCI_recvs to send and receive messages.
  LCI_short_t message;
  // Suppose we want to send an uint64_t.
  if (sizeof(uint64_t) > LCI_SHORT_SIZE) {
    fprintf(stderr,
            "The message is too long to be sent/received"
            "using LCI_sends/LCI_recvs");
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
  // The tag of the messages. Send and recv will be matched based on the tag
  // or rank+tag, depending on the matching rule. You can set the match rule by
  // LCI_plist_set_match_type.
  LCI_tag_t tag = 99;
  // User-defined contexts.
  void* user_context = (void*)9527;
  if (LCI_RANK == 0) {
    for (int i = 0; i < num_msgs; i++) {
      // Send a short message using LCI_sends.
      // A LCI send function can return LCI_ERR_RETRY, so we use a while loop
      // here to make sure the message is sent.
      while (LCI_sends(ep, message, peer_rank, tag) == LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      // Recv a short message using LCI_recvs
      LCI_recvs(ep, peer_rank, tag, sync, user_context);

      LCI_request_t request;
      // Test whether a synchronizer has been triggered.
      while (LCI_sync_test(sync, &request) == LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      // Once a synchronizer is triggered, the completion information will be
      // written to the request object.
      assert(request.flag == LCI_OK);
      assert(request.rank == peer_rank);
      assert(request.tag == tag);
      assert(request.type == LCI_IMMEDIATE);
      assert(request.user_context == user_context);
      assert(*(uint64_t*)&request.data.immediate == 9527 + peer_rank);
    }
  } else {
    for (int i = 0; i < num_msgs; i++) {
      LCI_recvs(ep, peer_rank, tag, sync, user_context);
      LCI_request_t request;
      while (LCI_sync_test(sync, &request) == LCI_ERR_RETRY)
        LCI_progress(device);
      assert(request.flag == LCI_OK);
      assert(request.rank == peer_rank);
      assert(request.tag == tag);
      assert(request.type == LCI_IMMEDIATE);
      assert(request.user_context == user_context);
      assert(*(uint64_t*)&request.data.immediate == 9527 + peer_rank);
      while (LCI_sends(ep, message, peer_rank, tag) == LCI_ERR_RETRY)
        LCI_progress(device);
    }
  }
  // Free all the resources
  LCI_sync_free(&sync);
  LCI_endpoint_free(&ep);
  LCI_device_free(&device);
  // Call `LCI_finalize` to finalize the runtime
  LCI_finalize();
  return 0;
}
