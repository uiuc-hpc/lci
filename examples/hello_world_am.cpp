// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <iostream>
#include <sstream>
#include <cassert>
#include "lci.hpp"

// This example shows the usages of basic communication operations (active
// message) and completion mechanisms (synchronizer and handler).

// Using a flag for simple termination detection.
bool received = false;

// Define the function to be triggered when the active message arrives.
void am_handler(lci::status_t status)
{
  // Get the active message payload.
  lci::buffer_t payload = status.get_buffer();
  std::string payload_str(static_cast<char*>(payload.base), payload.size);
  // Active message payload buffer is allocated by the LCI runtime using
  // `malloc` by default. The user are responsible for freeing it after use.
  free(payload.base);
  // Print the hello world message.
  std::ostringstream oss;
  oss << "Rank " << lci::get_rank_me() << " received active message from rank "
      << status.rank << ". Payload: " << payload_str << std::endl;
  std::cout << oss.str();
  // Set the received flag to true.
  received = true;
}

int main(int argc, char** args)
{
  lci::g_runtime_init();
  // We use "synchronizer" as the source side completion object.
  // It is similar to a MPI request, but has an optional argument `threshold` to
  // accept multiple signals before becoming ready.
  lci::comp_t sync = lci::alloc_sync();
  // Register the active message handler as the target side completion object.
  lci::comp_t handler = lci::alloc_handler(am_handler);
  // Since handler/cq needs to be referenced by another process, we need to
  // register it into a remote completion handler.
  // Since all ranks register the rcomp, all ranks will automatically have a
  // symmetric view. We do not need to explicitly exchange them.
  lci::rcomp_t rcomp = lci::register_rcomp(handler);

  // Put a barrier here to ensure all ranks have registered the handler
  lci::barrier();

  if (lci::get_rank_me() == 0) {
    for (int target = 0; target < lci::get_rank_n(); ++target) {
      // Prepare the active message payload.
      std::string payload =
          "Hello from rank " + std::to_string(lci::get_rank_me());
      // Post the active message to the target rank.
      auto send_buf =
          const_cast<void*>(static_cast<const void*>(payload.data()));
      // Unlike MPI_Isend, LCI posting operation can return a status in one of
      // the three states:
      // 1. `retry`: the posting failed due to resource being temporarily busy,
      // and the user can retry.
      lci::status_t status;
      do {
        status = lci::post_am(target, send_buf, payload.size(), sync, rcomp);
        lci::progress();
      } while (status.is_retry());
      // 2. `posted`: the operation is posted, and the completion object will be
      // signaled.
      if (status.is_posted()) {
        while (lci::sync_test(sync, &status /* can be nullptr */) == false) {
          lci::progress();
        }
        assert(status.is_done());
      }
      // 3. `done`: the operation is completed, the completion object will not
      // be signaled, and the user can check the status.
      assert(status.is_done());
      // at this point, all fields in the status object are valid.
    }
  }
  // Wait for the active message to arrive.
  while (!received) {
    lci::progress();
  }

  // Clean up the completion objects.
  lci::free_comp(&handler);
  lci::free_comp(&sync);

  lci::g_runtime_fina();
  return 0;
}
