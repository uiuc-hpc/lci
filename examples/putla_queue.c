#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

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

  // We can use one completion queue for both sends and receives.
  LCI_comp_t cq;
  LCI_queue_create(device, &cq);

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
  // We set the completion mechanism on the sender side to be completion queue.
  LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, LCI_COMPLETION_QUEUE);
  // We set the completion mechanism on the receiver side to be completion
  // queue. This also sets the completion type of LCI_DEFAULT_COMP_REMOTE.
  LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_QUEUE);
  // Set the default completion object to be triggered by
  // LCI_DEFAULT_COMP_REMOTE.
  LCI_plist_set_default_comp(plist, cq);
  // Matching rule does not matter here any more, as we are not using send/recv
  // LCI_plist_set_match_type(plist, LCI_MATCH_RANKTAG);
  LCI_endpoint_init(&ep, device, plist);
  LCI_plist_free(&plist);

  // For putla, we only need to allocate the send buffer. The receiver buffer
  // will be allocated by the LCI runtime and delivered to the users through
  // the completion object.
  LCI_lbuffer_t src_buf;
  src_buf.address = malloc(msgs_size);
  src_buf.length = msgs_size;
  memset(src_buf.address, 'a' + LCI_RANK, msgs_size);

  // Users need to explicitly register/deregister the send buffer.
  LCI_memory_register(device, src_buf.address, src_buf.length,
                      &src_buf.segment);
  // Alternatively, users can pass a LCI_SEGMENT_ALL and the LCI runtime will
  // register and deregister them for users.
  // src_buf.segment = LCI_SEGMENT_ALL;

  // Alternatively, users can directly use LCI_lbuffer_alloc to allocate and
  // register the buffers.
  // LCI_lbuffer_alloc(device, msgs_size, &src_buf);

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
  // User-defined contexts.
  void* user_context = (void*)9527;
  if (LCI_RANK == 0) {
    for (int i = 0; i < num_msgs; i++) {
      // Send a long message using LCI_putla
      // A LCI send function can return LCI_ERR_RETRY, so we use a while loop
      // here to make sure the message is sent
      while (LCI_putla(ep, src_buf, cq, peer_rank, tag, LCI_DEFAULT_COMP_REMOTE,
                       user_context) == LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);

      LCI_request_t request;
      // Try to pop a entry from the completion queue.
      while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      // Once an entry is popped, the completion information will be
      // written to the request object.
      assert(request.flag == LCI_OK);
      assert(request.rank == peer_rank);
      assert(request.tag == tag);
      assert(request.type == LCI_LONG);
      assert(request.user_context == user_context);
      assert(request.data.lbuffer.address == src_buf.address);
      assert(request.data.lbuffer.length == src_buf.length);
      assert(request.data.lbuffer.segment == src_buf.segment);

      // Wait for a remote put from the peer.
      // We do not need to post a recv here. The data will directly be delivered
      // throught the default completion object.
      // Try to pop a entry from the completion queue.
      while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY)
        // Users have to call LCI_progress frequently to make progress on the
        // background work.
        LCI_progress(device);
      // Once an entry is popped, the completion information will be
      // written to the request object.
      assert(request.flag == LCI_OK);
      assert(request.rank == peer_rank);
      assert(request.tag == peer_tag);
      assert(request.type == LCI_LONG);
      assert(request.data.lbuffer.length == msgs_size);
      for (int j = 0; j < msgs_size; ++j) {
        assert(((char*)request.data.lbuffer.address)[j] == 'a' + peer_rank);
      }
      // Since we are using the "a" (allocate) version of LCI_put, the receive
      // buffer is allocated by the LCI runtime. We need to return it to the
      // runtime after we processing the data.
      LCI_lbuffer_free(request.data.lbuffer);
    }
  } else {
    for (int i = 0; i < num_msgs; i++) {
      LCI_request_t request;
      while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY) LCI_progress(device);
      assert(request.flag == LCI_OK);
      assert(request.rank == peer_rank);
      assert(request.tag == peer_tag);
      assert(request.type == LCI_LONG);
      assert(request.data.lbuffer.length == msgs_size);
      for (int j = 0; j < msgs_size; ++j) {
        assert(((char*)request.data.lbuffer.address)[j] == 'a' + peer_rank);
      }
      LCI_lbuffer_free(request.data.lbuffer);

      while (LCI_putla(ep, src_buf, cq, peer_rank, tag, LCI_DEFAULT_COMP_REMOTE,
                       user_context) == LCI_ERR_RETRY)
        LCI_progress(device);
      while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY) LCI_progress(device);
      assert(request.flag == LCI_OK);
      assert(request.rank == peer_rank);
      assert(request.tag == tag);
      assert(request.type == LCI_LONG);
      assert(request.user_context == user_context);
      assert(request.data.lbuffer.address == src_buf.address);
      assert(request.data.lbuffer.length == src_buf.length);
      assert(request.data.lbuffer.segment == src_buf.segment);
    }
  }
  // Free all the resources
  // If you are using LCI_lbuffer_alloc
  // LCI_lbuffer_free(src_buf);
  // If you are using LCI_SEGMENT_ALL
  // assert(src_buf.segment == LCI_SEGMENT_ALL);
  LCI_memory_deregister(&src_buf.segment);
  free(src_buf.address);
  LCI_endpoint_free(&ep);
  LCI_device_free(&device);
  // call `LCI_finalize` to finalize the runtime
  LCI_finalize();
  return 0;
}
