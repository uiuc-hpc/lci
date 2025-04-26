# LCI
A Lightweight Communication Interface for Asynchronous Multithreaded Systems

![Build Status](https://github.com/uiuc-hpc/lci/actions/workflows/CI/badge.svg)

## Overview

The Lightweight Communication Interface (LCI) is designed to be an efficient communication library
for asynchronous communications in multithreaded environments. It also serves as a research tool to 
explore design choices for such libraries. It has the following major features:
- a unified interface that supports flexible combinations of all common point-to-point 
  communication primitives, including send-receive, active messages, and 
  RMA put/get (with/without notification), and various built-in mechanisms to synchronize 
  with pending communications, including synchronizers, completion queues, function handlers, 
  and completion graphs.
- a flexible interface offers both a simple starting point for users to program and a wide range of options 
  for them to incrementally fine-tune the communication resources and runtime behaviors, 
  minimizing potential interference between communication and computation.
- a lightweight and efficient runtime optimized for threading efficiency and massive parallelism. 
  The runtime is built with a deep understanding of low-level network activities and employs optimizations 
  such as atomic-based data structures, thread-local storage, and fine-grained nonblocking locks.

LCI is implemented as a C++ libraries with two major network backends: 
[libibverbs](https://github.com/linux-rdma/rdma-core/blob/master/Documentation/libibverbs.md) for InfiniBand/RoCE and 
[libfabric](https://ofiwg.github.io/libfabric/) for Slingshot-11, Ethernet, shared memory, and other networks.

[API documentation](https://uiuc-hpc.github.io/lci/)

## Installation
### CMake

```
git clone git@github.com:uiuc-hpc/lci.git
cd lci
mkdir build
cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
make
make install
```

#### Important CMake variables
- `LCI_DEBUG=[ON|OFF]`: Enable/disable the debug mode (more assertions and logs).
  The default value is `OFF`.
- `LCI_NETWORK_BACKENDS=[ibv|ofi]`: allow multiple values separated with comma.
  Hint to which network backend to use. 
  If the backend indicated by this variable are found, LCI will just use it.
  Otherwise, LCI will use whatever are found with the priority `ibv` > `ofi`.
  The default value is `ibv,ofi`. Typically, you don't need to
  modify this variable as if `libibverbs` presents, it is likely to be the recommended one to use.
  - `ibv`: [libibverbs](https://github.com/linux-rdma/rdma-core/blob/master/Documentation/libibverbs.md), 
    typically for infiniband/RoCE.
  - `ofi`: [libfabric](https://ofiwg.github.io/libfabric/), 
    for all other networks (slingshot-11, ethernet, shared memory). 

### Spack
LCI can be installed using [Spack](https://spack.io/).
```
git clone git@github.com:uiuc-hpc/lci.git --branch=lci2
spack repo add lci/contrib/spack
spack install lci
```

## Run LCI applications

We use the same mechanisms as MPI to launch LCI processes, so you can use the same way
you run MPI applications to run LCI applications. Typically, it would be `mpirun` or
`srun`. For example,
```
mpirun -n 2 ./hello_world
```
or
```
srun -n 2 ./hello_world
```

In addition, you can use the `lcrun` script
```
lcrun -n 2 ./hello_world
```

We do not expect `lcrun` to have the same level of scalability as `mpirun` or `srun`,
but it is a good tool for fast testing and debugging.

## Write an LCI program

Read [this short paper](https://arxiv.org/abs/2503.15400) to understand the high-level interface design of LCI. 

See `examples` and `tests` for some example code.

Check out the [API documentation](https://uiuc-hpc.github.io/lci/) for more details.

## LICENSE
See LICENSE file.

## Authors

- \@danghvu (danghvu@gmail.com)
- \@omor1
- \@JiakunYan (jiakunyan1998@gmail.com)
- \@snirmarc

## Relevant Publications
- Yan, Jiakun, and Marc Snir. "Contemplating a Lightweight Communication Interface for Asynchronous Many-Task Systems." 
arXiv preprint arXiv:2503.15400 (2025). 
*A short workshop paper describing the high-level interface design of LCI (version 2, the current version).*
- Yan, Jiakun, Hartmut Kaiser, and Marc Snir. "Understanding the Communication Needs of Asynchronous Many-Task Systems
--A Case Study of HPX+ LCI." arXiv preprint arXiv:2503.12774 (2025). 
*Paper about integrating LCI (version 1) into HPX.*
- Mor, Omri, George Bosilca, and Marc Snir. "Improving the scaling of an asynchronous many-task runtime with 
a lightweight communication engine." Proceedings of the 52nd International Conference on Parallel Processing. 2023. 
*Paper about integrating LCI (version 1) into PaRSEC.*

## Frequently Asked Questions
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
