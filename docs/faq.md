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
#### What is LCT?
The Lightweight Communication Tools (LCT) library provides basic services such as bootstrapping
and logging for LCI. It is a C++ library that can be used without LCI. You can build LCT without
LCI by setting `LCI_WITH_LCT_ONLY=ON` (cmake variable).
