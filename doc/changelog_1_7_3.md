@page Changelog_1_7_3 Changelog v1.7.3

# Change log for LCI v1.7.3

## Major Changes
- Integrate PAPI into LCI.
  - Use cmake option LCI_USE_PAPI to enable.
  - Use env var LCI_PAPI_EVENTS to specify events to monitor.
- Add pmix support for LCI.
- Update README, doxygen comments, and example codes.

## Breaking Changes
- Change uint32_t rank to int rank in sendl/recvl to match other
  communication operations.

## Other Changes
- Add cmake option LCI_IBV_ENABLE_TRY_LOCK_QP.
- Use thread domain when try_lock is enabled.
- Add more performance counters.
- Only print performance counter data when 
  LCI_USE_PERFORMANCE_COUNTERS is enabled.
- Add cmake option LCI_ENABLE_SLOWDOWN and env var 
  LCI_SEND_SLOW_DOWN_USEC and LCI_RECV_SLOW_DOWN_USEC: 
  manually slow down the messages.
- Change the putva rendezvous protocol: first send FIN and then 
  deregister memory.
- Make LCI_PACKET_SIZE configurable at runtime.
- Add some log to help debug the hang (if there is any) in the 
  initialization phase.
- Only do fetch_and_add on g_next_rdma_key when FI_MR_PROV_KEY is 
  not set.
- Use the msg_comp_type as the remote default completion type 
  instead of the hardcoded LCI_COMP_QUEUE.
- Use CSV format for performance counter printing outputs.

## Fixed Issues
- Fix LCIU_timespec_diff.
- Fix a few alignment bugs.
- Fix a bug in ibv backend when td is not supported.
- Fix a putva bug: putva send buffers might never get freed.
- Fix LCI_TOUCH_LBUFFER when allocating long buffers.
- Fix compiler non-void function no return error.
- Provides workaround for libfabric/cxi fi_mr_bind bug.
- Fix the handler completion mechanism.