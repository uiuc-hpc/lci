#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

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

  // Create a synchronizer. You can treat it as an MPI_Request for now.
  // Setting the threshold to 1 means it will be triggered by the completion
  // of one asynchronous operation.
  LCI_comp_t sync;
  LCI_sync_create(device, 1, &sync);

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
  // LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, LCI_COMPLETION_SYNC);
  // We set the completion mechanism on the receiver side to be synchronizer.
  // This also sets the completion type of LCI_DEFAULT_COMP_REMOTE.
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC);
  // Set the default completion object to be triggered by
  // LCI_DEFAULT_COMP_REMOTE.
  LCI_plist_set_default_comp(plist, sync);
  // Matching rule does not matter here any more, as we are not using send/recv
  // LCI_plist_set_match_type(plist, LCI_MATCH_RANKTAG);
  LCI_endpoint_init(&ep, device, plist);
  LCI_plist_free(&plist);

  // For medium messages (up to LCI_MEDIUM_SIZE bytes), we can use LCI_putm(n)a
  // to directly put messages on the target process. The difference between the
  // "copy" version (LCI_putma) and the "no copy" version (LCI_putmna) is
  // whether the send buffers are provided by users or the LCI runtime.
  if (msgs_size > LCI_MEDIUM_SIZE) {
    fprintf(stderr,
            "The message is too long to be sent/received"
            "using LCI_putmna");
    exit(1);
  }

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
  if (LCI_RANK == 0) {
    for (int i = 0; i < num_msgs; i++) {
      // We are using LCI_putmna in this example, so we need to ask the LCI
      // runtime for a pre-allocated and pre-registered internal buffer (we call
      // it packet) whenever we want to invoke a put.
      LCI_mbuffer_t src_buf;
      // LCI_mbuffer_alloc can fail due to temporarily unavailable resources,
      // so we need a while loop here.
      while (LCI_mbuffer_alloc(device, &src_buf) == LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      src_buf.length = msgs_size;
      memset(src_buf.address, 'a' + LCI_RANK, msgs_size);
      // Send a medium message using LCI_putmna
      // A LCI send function can return LCI_ERR_RETRY, so we use a while loop
      // here to make sure the message is sent
      while (LCI_putmna(ep, src_buf, peer_rank, tag, LCI_DEFAULT_COMP_REMOTE) ==
             LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      // The send buffer will be directly returned to the runtime after the
      // "n" (no-copy) version of send/put completes.

      LCI_request_t request;
      // Wait for a remote put from the peer.
      // We do not need to post a recv here. The data will directly be delivered
      // throught the default completion object.
      // Test whether a synchronizer has been triggered.
      while (LCI_sync_test(sync, &request) == LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      // Once a synchronizer is triggered, the completion information will be
      // written to the request object.
      assert(request.flag == LCI_OK);
      assert(request.rank == peer_rank);
      assert(request.tag == peer_tag);
      assert(request.type == LCI_MEDIUM);
      assert(request.data.mbuffer.length == msgs_size);
      for (int j = 0; j < msgs_size; ++j) {
        assert(((char*)request.data.mbuffer.address)[j] == 'a' + peer_rank);
      }
      // Since we are using the "a" (allocate) version of LCI_put, the receive
      // buffer is allocated by the LCI runtime. We need to return it to the
      // runtime after we processing the data.
      LCI_mbuffer_free(request.data.mbuffer);
    }
  } else {
    for (int i = 0; i < num_msgs; i++) {
      LCI_request_t request;
      while (LCI_sync_test(sync, &request) == LCI_ERR_RETRY)
        LCI_progress(device);
      assert(request.flag == LCI_OK);
      assert(request.rank == peer_rank);
      assert(request.tag == 99 + peer_rank);
      assert(request.type == LCI_MEDIUM);
      assert(request.data.mbuffer.length == msgs_size);
      for (int j = 0; j < msgs_size; ++j) {
        assert(((char*)request.data.mbuffer.address)[j] == 'a' + peer_rank);
      }
      // Alternatively, we can directly reuse the packet for the next send/put.
      LCI_mbuffer_t src_buf = request.data.mbuffer;
      src_buf.length = msgs_size;
      memset(src_buf.address, 'a' + LCI_RANK, msgs_size);
      while (LCI_putmna(ep, src_buf, peer_rank, tag, LCI_DEFAULT_COMP_REMOTE) ==
             LCI_ERR_RETRY)
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
