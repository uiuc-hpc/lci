@page Changelog_2_0_0 Changelog v2.0.0

# Change log for LCI v2.0.0

LCI v2.0.0 is the first stable release of LCI version 2. LCI 2 is a
redesign of LCI 1.x with a new C++ interface. See the
[SC '25 paper](https://arxiv.org/abs/2505.01864) and the @ref tutorial for an
introduction. This changelog covers changes since v2.0.0-beta.1.

From this release on, LCI follows semantic versioning for the public API in
`lci.hpp`. Features marked experimental are excluded from this guarantee.

## Breaking Changes (since v2.0.0-beta.1)

### Remote memory and RMA
- Replace `rkey_t` (a `uint64_t`) with `rmr_t`, a struct describing a remote memory region; add `RMR_NULL`; `get_rkey` becomes `get_rmr`. Code that sent rkeys as 64-bit integers must send `rmr_t` instead.
- RMA operations take a displacement relative to the remote memory region instead of a remote address: the `remote_buffer` argument of `post_comm`/`post_put`/`post_get` becomes `remote_disp`. Network-layer `post_put(s)`/`post_putImm(s)`/`post_get` drop their `base` argument.

### Buffers and completion status
- Remove `data_t`, `buffer_t`, `buffers_t`, `rbuffer_t`, and `rbuffers_t`, and the `buffers`/`rbuffers` arguments of `post_comm`.
- `status_t` replaces `data` with `buffer` (`void*`) and `size`. `get_buffer()` returns `void*`. Remove `get_scalar<T>`, `get_buffers`, `is_scalar`, `is_buffer`, and `is_buffers`. Add `get_error`, `get_rank`, `get_size`, `get_tag`, and `get_user_context`.
- Rename `comp_semantic_t::buffer` to `comp_semantic_t::memory`.
- Remove the `force_zcopy` argument.

### Completion objects
- `COMP_NULL` (`allow_posted` and `allow_retry` false) and `COMP_NULL_RETRY` (`allow_posted` false) replace `COMP_NULL_EXPECT_DONE` and `COMP_NULL_EXPECT_DONE_OR_RETRY`, which remain as deprecated aliases. Their numeric values changed, so code compiled against beta.1 must be recompiled.
- A default-constructed `comp_t()` now equals `COMP_NULL`.
- `sync_wait` calls `progress()` by default (new optional arguments `do_progress` and `device`).

### Runtime behavior
- Freeing an endpoint no longer performs a global barrier; it only drains local operations. As a result, `free_device`/`free_runtime`/`g_runtime_fina` are no longer implicit global barriers. Call `barrier()` before teardown if your application relied on that.
- The rendezvous protocol (`rdv_protocol`) defaults to `auto_select`, which uses write-with-immediate (`writeimm`) when the network supports it, on both ibv and ofi.
- The environment variable selecting the network backend is renamed from `LCI_NETWORK_BACKEND_DEFAULT` to `LCI_ATTR_BACKEND`. Requesting an unavailable backend is now a fatal error.
- `LCI_NET_TRYLOCK_MAX` changes value (new lock-mode flags were added).
- `get_rank_me_x`/`get_rank_n_x` become plain functions; `g_runtime_init` returns `runtime_t`.

### Build and packaging
- Network backend libraries are linked `PRIVATE`. Applications that used libibverbs/libfabric directly must link them themselves.
- Replace `LCI_COMPILE_REG_CACHE`/`LCI_USE_REG_CACHE_DEFAULT` with `LCI_USE_REG_CACHE` (default OFF; not supported on macOS).
- `-Werror` is only applied when LCI is the top-level project (`LCI_BUILD_WARN_AS_ERROR`).
- `LCIConfigVersion.cmake` uses `SameMajorVersion` compatibility, so `find_package(LCI 2.0)` accepts later 2.x releases.
- Fix the installed `liblci.pc`, which pkg-config previously rejected.

### LCT
- The default PMI backend list is `pmix;pmi2;pmi1;mpi;tcp;file;local`. The TCP backend is selected automatically when `RANK`, `WORLD_SIZE`, and `MASTER_ADDR` are set (e.g. under `torchrun`).
- `LCT_pmi_initialize`/`LCT_pmi_finalize` are reference-counted.
- Log lines include an elapsed-time field.

### License
- LCI is now distributed under the NCSA license (previously MIT).

## Major Changes
- GPU Direct RDMA (new since beta.1)
  - Build with `LCI_USE_CUDA` or `LCI_USE_HIP` (mutually exclusive).
  - Memory-type hints `MR_HOST`/`MR_DEVICE`/`MR_UNKNOWN`; DMA-BUF support (`use_dmabuf`).
  - GPU Direct RDMA for small messages. See @ref gpu_direct_rdma.
- Collectives
  - Add `allgather`, `allreduce`, `reduce_scatter`, and nonblocking `alltoall`.
  - Nonblocking `broadcast` and `barrier`; selectable algorithms (`broadcast_algorithm_t`, `reduce_scatter_algorithm_t`, `allreduce_algorithm_t`).
- Completion objects
  - Add the `counter` completion object (`alloc_counter`, `counter_get`, `counter_set`).
  - Add `wait_drained`/`test_drained`.
  - Support user-defined completion objects (`attr_comp_type_t::custom`).
- Failure reporting
  - Report the peer rank for asynchronous completion failures: `peer_failure_error` (with `failed_rank()`) and `net_opcode_t::ERROR` from `net_poll_cq`.
  - Use abortive teardown after network failures.
- Runtime and configuration
  - Add `global_initialize`/`global_finalize` and `set_g_default_attr`/`get_g_default_attr`.
  - Add backend-agnostic network device selection (`device_name`).
  - Automatically select the network backend.
  - Add an optional memory registration cache (`LCI_USE_REG_CACHE`, `use_reg_cache`).
  - Add a dedicated progress endpoint (`alloc_progress_endpoint`).
  - Accept textual boolean values (`true`/`false`, `yes`/`no`, `on`/`off`) in environment variables.
- Network backends
  - ibv: reserve send-queue slots for high-priority messages (`net_send_reserved_pct`); split endpoints from devices.
  - ofi: add the `mr` and `all` values for `ofi_lock_mode`; enable CXI write-with-immediate; allow HMEM providers without injection support; let providers choose the address vector type; use allgather for bootstrap.
- **Experimental**: intra-node shared-memory transport for small messages. Build with `-DLCI_WITH_SHM=ON` (default OFF); enable at runtime with `shm_enable`.
- Bootstrap (LCT PMI)
  - Add a TCP PMI backend compatible with `torchrun`.
  - Reuse an already initialized LCT PMI in LCI bootstrap; use PMI for endpoint readiness.
  - Add `LCT_pmi_abort`.
  - `lcrun` isolates the file PMI state of each invocation and terminates the remaining ranks when one fails.

## Other Changes
- Add examples (distributed array) and benchmarks (`bench_am_lat`, `bench_put_bw`, `bench_many2many_am`, `bench_get_buffer_attr`).
- Spack package: add the `bootstrap` variant. Fix the `npackets` and `fabric-n{sends,recvs,cqes}-max` variants being ignored for LCI 2. Reject `fabric=ucx` for LCI 2, which has no UCX backend.
- Fix the declarations of `get_reduce_scatter_algorithm_str` and `get_allreduce_algorithm_str`.
- Fixes for packet loss with user-provided packets, a deadlock when allocating multiple devices, double completion signals, and C23 compatibility of the vendored UCS code used by the registration cache.
- Add CodeQL and downstream CI (LCW, Reconverse).
