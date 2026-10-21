@page Changelog_2_0_0 Changelog v2.0.0

# Change log for LCI v2.0.0

LCI v2.0.0 is the first stable release of LCI version 2. LCI 2 is a
redesign of LCI 1.x with a new C++ interface. See the
[SC '25 paper](https://arxiv.org/abs/2505.01864) and the @ref tutorial for an
introduction. This changelog covers changes since v2.0.0-beta.1.

From this release on, LCI follows semantic versioning for the public API in
`lci.hpp`. Features marked experimental are excluded from this guarantee.

## Breaking Changes (since v2.0.0-beta.1)
- RMA operations take a displacement relative to the remote memory region instead of a remote address.
- Rename `rkey` to `rmr` (remote memory region).
- Remove `data_t` and `buffer_t`. Remove the `buffers`/`rbuffers` arguments from `post_comm`.
- Rename `comp_semantic_t::buffer` to `comp_semantic_t::memory`.
- Remove the `force_zcopy` argument.
- Add `COMP_NULL` and `COMP_NULL_RETRY`. `COMP_NULL_EXPECT_DONE` and `COMP_NULL_EXPECT_DONE_OR_RETRY` are deprecated aliases.

## Major Changes
- GPU Direct RDMA
  - Add HIP/ROCm support (`LCI_USE_HIP`) alongside CUDA (`LCI_USE_CUDA`).
  - Enable GPU Direct RDMA for small messages; support DMA-BUF on ibv.
- Collectives
  - Add `allgather`, `allreduce`, `reduce_scatter`, nonblocking `alltoall`, and more `broadcast` algorithms.
- Completion objects
  - Add the `counter` completion object (`alloc_counter`, `counter_get`, `counter_set`).
  - Add `wait_drained`/`test_drained`.
- Runtime and configuration
  - Add `global_initialize`/`global_finalize` and `set_g_default_attr`/`get_g_default_attr`.
  - Add backend-agnostic network device selection (`device_name`).
  - Automatically select the network backend.
  - Add an optional memory registration cache (`LCI_USE_REG_CACHE`, `use_reg_cache`).
  - Accept textual boolean values (`true`/`false`, `on`/`off`, ...) in environment variables.
- Network backends
  - ibv: add a rendezvous protocol based on write-with-immediate; reserve send-queue slots for high-priority messages; split endpoints from devices.
  - ofi: add the OFI MR lock mode (`ofi_lock_mode`); enable CXI write-with-immediate; allow HMEM providers without injection support; let providers choose the address vector type; use allgather for bootstrap.
  - Report the peer rank for asynchronous completion failures and use abortive teardown after network failures.
- **Experimental**: intra-node shared-memory transport for small messages. Build with `-DLCI_WITH_SHM=ON` (default OFF); runtime attribute `shm_enable`.
- Bootstrap (LCT PMI)
  - Add a TCP PMI backend compatible with `torchrun`.
  - Reuse an already initialized LCT PMI in LCI bootstrap; use PMI for endpoint readiness.
  - Add `LCT_pmi_abort`.
  - Isolate the file PMI state of each `lcrun` invocation.

## Other Changes
- Add examples (distributed array) and benchmarks (`bench_am_lat`, `bench_put_bw`, `many2many_am`, `bench_get_buffer_attr`).
- Spack package: add the `bootstrap` variant. Fix the `npackets` and `fabric-n{sends,recvs,cqes}-max` variants being ignored for LCI 2.
- Fixes for packet loss with user-provided packets, a deadlock when allocating multiple devices, double completion signals, and C23 compatibility of the UCX backend.
- Add CodeQL and downstream CI (LCW, Reconverse).
