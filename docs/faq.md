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

The OFI and IBV backends report some asynchronous peer failures from
`progress()` as `lci::peer_failure_error`. It remains catchable as
`std::runtime_error`, and its `failed_rank()` method identifies the peer:

```cpp
try {
  lci::progress_x().device(device)();
} catch (const lci::peer_failure_error& error) {
  std::cerr << "Peer " << error.failed_rank()
            << " failed: " << error.what() << '\n';
}
```

`peer_failure_error` is not reported for every asynchronous failure. For
unsupported failure cases, `progress()` throws a generic `std::runtime_error`
instead. Applications that need the failed rank should catch
`peer_failure_error`, but should also handle `std::runtime_error`.

Neither exception repairs membership, replaces the peer, or completes
operations that were in flight to that peer. Applications must decide how to
recover after catching an exception. `progress()` processes a full batch of
completed operations before throwing, so successful operations in the same
batch are still processed.

After an asynchronous network failure is reported, LCI marks the affected
device as failed. Destroying an endpoint on that device uses abortive teardown:
it does not wait for outstanding operations that the failed transport may
never complete. Those operations may be abandoned without signaling their
completion objects. Normal endpoint teardown still waits for all operations to
drain when no network failure has been observed.

The `test-resilience-process-failure` CTest program exercises this path with
the libfabric TCP provider: it kills rank 1, verifies that rank 0 receives a
`peer_failure_error` for rank 1, and confirms that the two surviving ranks
continue progressing and finalize. Run it on a host with an OFI build and a
libfabric TCP provider.


#### What is LCT?
The Lightweight Communication Tools (LCT) library provides basic services such as bootstrapping
and logging for LCI. It is a C++ library that can be used without LCI. You can build LCT without
LCI by setting `LCI_WITH_LCT_ONLY=ON` (cmake variable).
