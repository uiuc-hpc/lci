\page faq Frequently Asked Questions

[TOC]

### Bootstrapping
#### Why was my LCI application launched as a collection of processes all with rank 0?
LCI was not bootstrapped correctly by `srun` or `mpirun`. LCI primarily relies on the
Process-Management Interface (PMI) to bootstrap, the same way as MPI. LCI supports
PMI1, PMI2, and PMIx. The LCI source code is shipped with a copy of the SLURM PMI1
and PMI2 client implementation, so users can normally use LCI on SLURM without any
extra configuration. However, if `srun` does not enable PMI by default, or
enables PMIx, or you use `mpirun`, additional configuration might be needed.

If you are using `srun`, you can explicitly enable PMI1 or PMI2 by using the `--mpi`
option.
```
srun --mpi=pmi2 -n 2 ./hello_world
```

If you are using `mpirun`, you need to find the corresponding PMI client library
and link LCI to it.
```
# Find the PMI client library
ldd $(which mpirun)
```

Normally, MPICH uses `hydra-pmi`; Cray-MPICH uses `cray-pmi`; OpenMPI uses `pmix`.
After finding the PMI client library, you can reconfigure LCI with the corresponding
PMI client library through the `PMI_ROOT`, `PMI2_ROOT`, or `PMIx_ROOT` environment/cmake
variables.

As a last resort, you can also set -DLCT_PMI_BACKEND_ENABLE_MPI=ON and link LCI to MPI. 
In this case, LCI will just use MPI to bootstrap. Performance might be slightly impacted 
as LCI and MPI can contend for network resources, but the impact should be insignificant.

You can use `export LCT_LOG_LEVEL=info` to monitor what bootstrap backend LCI is actually using 
and use `export LCT_PMI_BACKEND=[pmi1|pmi2|pmix|mpi]` to change the default behavior.

#### Why does `lcrun` not work?
`lcrun` relies on another simple bootstrapping backend `file` which relies on the shared file
system and `flock` to work.

It is possible that a previous failed run of `lcrun` did not clean up the temporary files
it created. You can
```
rm -r ~/.tmp/lct_pmi_file-*
```
then try again.

### Others
#### How do I identify a failed LCI peer?

The OFI and IBV backends report an asynchronous failure from `progress()` as
`lci::peer_failure_error` when it belongs to a simple, one-completion outgoing
LCI operation and the peer can be identified. It remains catchable as
`std::runtime_error`, and its `failed_rank()` method identifies the peer:

```cpp
try {
  lci::progress_x().device(device)();
} catch (const lci::peer_failure_error& error) {
  std::cerr << "Peer " << error.failed_rank()
            << " failed: " << error.what() << '\n';
}
```

This reports a transport failure; it does not repair membership, replace the
peer, or complete operations that were in flight to that peer. Applications
must decide how to recover after catching it. The
`test-resilience-process-failure` CTest program exercises this path with the
libfabric TCP provider: it kills rank 1, verifies that rank 0 receives a
`peer_failure_error` for rank 1, and confirms that the two surviving ranks
continue progressing and finalize. Run it on a host with an OFI build and a
libfabric TCP provider.

Recovery is best effort. `progress()` consumes and processes the full polled
batch before throwing once, so successful completions in the same batch still
run and other failed simple operations are reclaimed when they can be
recognized. Their completion objects are not signaled as successful.
Rendezvous, split, fallback, control, receive, and otherwise unrecognized
failures remain generic and nonrecoverable; catching an exception does not
repair or drain those protocols. Synchronous post failures are unchanged.

The lower-level `net_*` API reports asynchronous failures as
`net_opcode_t::ERROR` statuses. IBV supplies a QP-derived rank for outgoing
errors when available; OFI generally supplies rank `-1` and preserves the
outgoing `user_context`. Receive errors use rank `-1` and a null context.
Applications should not mix raw network operations with `progress()` on the
same device.

#### What is LCT?
The Lightweight Communication Tools (LCT) library provides basic services such as bootstrapping
and logging for LCI. It is a C++ library that can be used without LCI. You can build LCT without
LCI by setting `LCI_WITH_LCT_ONLY=ON` (cmake variable).
