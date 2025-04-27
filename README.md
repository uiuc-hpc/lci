# LCI

A Lightweight Communication Interface for Asynchronous Multithreaded Systems

![Build Status](https://github.com/uiuc-hpc/lci/actions/workflows/ci.yml/badge.svg)

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