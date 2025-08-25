# LCI

A Lightweight Communication Interface for Asynchronous Multithreaded Systems

![Build Status](https://github.com/uiuc-hpc/lci/actions/workflows/ci.yml/badge.svg)

## Documentation
Check [the LCI homepage](https://uiuc-hpc.github.io/lci/) for quick start, tutorial, API documentation, and more.

## Overview
LCI is designed to be an efficient communication library
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

## Performance Showcase

![Message Rate Result](docs/img/lt-expanse.png)
![Bandwidth Result](docs/img/bw-expanse.png)
![legend](docs/img/legend-expanse.png)

Results of multi-pair ping-pong microbenchmarks measuring message rate (8-byte messages, 100k steps) and 
bandwidth (64-thread, 1k steps) on SDSC Expanse (InfiniBand). It compares LCI with other state-of-the-art 
libraries, including MPI (MPICH), MPI with the VCI extension, and GASNet-EX. The experiments are either 
run with one process per core (multi-proc) or with one process per node (one thread per core, multi-thrd).

*For the first time known to us, multithreaded communication catches up with multi-process communication.*

## Publications
- Jiakun Yan, and Marc Snir. "LCI: a Lightweight Communication Interface for Efficient Asynchronous Multithreaded Communication." In The International Conference for High Performance Computing, Networking, Storage and Analysis (SC ’25), 2025.
*Users are recommended to read this paper for a comprehensive understanding of LCI (version 2).* [link](https://arxiv.org/abs/2505.01864)

### Other Relevant Ones
- Yan, Jiakun, Hartmut Kaiser, and Marc Snir. "Understanding the Communication Needs of Asynchronous Many-Task Systems
--A Case Study of HPX+ LCI." arXiv preprint arXiv:2503.12774 (2025). 
*Paper about integrating LCI (version 1) into HPX.*
- Mor, Omri, George Bosilca, and Marc Snir. "Improving the scaling of an asynchronous many-task runtime with 
a lightweight communication engine." Proceedings of the 52nd International Conference on Parallel Processing. 2023. 
*Paper about integrating LCI (version 1) into PaRSEC.*
- Dang, Hoang-Vu, Marc Snir, and William Gropp. "Towards millions of communicating threads." Proceedings of the 23rd European MPI Users' Group Meeting. 2016.
*Paper about a MPI variant that LCI was initially based on.*