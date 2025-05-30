@page tutorial Tutorial

[TOC]

# Add LCI to your project

The following CMake code will add LCI to your project. It will first try to find LCI on your system. If it is not found, it will download and build LCI from GitHub.

```cmake
# Try to find LCI externally
find_package(
  LCI
  CONFIG
  PATH_SUFFIXES
  lib/cmake
  lib64/cmake)
if(NOT LCI_FOUND)
  message(STATUS "Existing LCI installation not found. Try FetchContent.")
  include(FetchContent)
  FetchContent_Declare(
    lci
    GIT_REPOSITORY https://github.com/uiuc-hpc/lci.git
    GIT_TAG master)
  FetchContent_MakeAvailable(lci)
endif()

# Link LCI to your target
target_link_libraries(your_target PRIVATE LCI::lci)
```

# Install LCI

Although CMake FetchContent is convenient, you may also want to install LCI on your systems.

## Prerequisites
- A Linux or MacOS laptop or cluster.
- A C++ compiler that supports C++17 or higher (GCC 8 or higher, Clang 5 or higher, etc).
- CMake 3.12 or higher.
- A network backend that supports LCI. Currently, LCI supports:
  - [libibverbs](https://github.com/linux-rdma/rdma-core/blob/master/Documentation/libibverbs.md). Typically for Infiniband/RoCE.
  - [libfabric](https://ofiwg.github.io/libfabric/). For Slingshot-11, Ethernet, shared memory (including laptop), and other networks.

Normal clusters should already have these installed (you may need a few `module load`). If you are using a laptop, you may need to install cmake and libfabric manually.
- For Ubuntu, you can install the prerequisites using:
  ```bash
  sudo apt-get install -y cmake libfabric-bin libfabric-dev
  ```
- For MacOS, you can install the prerequisites using:
  ```bash
  brew install cmake libfabric
  ```

## With CMake
### Install LCI on your laptop

```bash
git clone https://github.com/uiuc-hpc/lci.git
cd lci
mkdir build
cmake -DCMAKE_INSTALL_PREFIX=/path/to/install -DLCI_NETWORK_BACKENDS=ofi ..
make
make install
```

### Install LCI on a cluster

```bash
git clone https://github.com/uiuc-hpc/lci.git
cd lci
mkdir build
cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
make
make install
```
Basically the same as installing LCI on your laptop, but you don't need to install pre-requisites
because they are already installed on the cluster. You also don't need to manually select the network backend because LCI will automatically select the best one for you.

### Important CMake variables
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

## With Spack
LCI can be installed using [Spack](https://spack.io/). An LCI recipe has been included in the Spack official repository. For the most up-to-date version, you can also use the recipe in the `contrib/spack` directory of the LCI repository. To install LCI using Spack, run the following commands:

```bash
# Assume you have already installed Spack
# Optional: add the LCI spack recipe from the LCI repository
git clone https://github.com/uiuc-hpc/lci.git
spack repo add lci/contrib/spack
spack install lci
```

### Important Spack variables
- `debug`: define the CMake variable `LCI_DEBUG`.
- `backend=[ibv|ofi]`: define the CMake variable `LCI_NETWORK_BACKENDS`.

## Cluster-specific Installation Note
### NCSA Delta

<details>
<summary>Click to expand</summary>

tl;dr: `module load libfabric`, then specify the cmake variable `-DLCI_NETWORK_BACKENDS=ofi` or the Spack variable `backend=ofi` when building LCI.

The only caveat is that you need to pass the -DLCI_NETWORK_BACKENDS=ofi option to CMake. This is because Delta somehow has both libibverbs and libfabric installed, but only libfabric is working.

No additional `srun` arguments are needed to run LCI applications. However, we have noticed that `srun` can be broken under some mysterious module loading conditions. In such case, just use `srun --mpi=pmi2` instead.
</details>

### SDSC Expanse

<details>
<summary>Click to expand</summary>
tl;dr: use `srun --mpi=pmi2` to run LCI applications.

You don't need to do anything special to install LCI on Expanse. Just follow the instructions above.
</details>

### NERSC Perlmutter
<details>
<summary>Click to expand</summary>
tl;dr: `module load cray-pmi` before running the CMake command. Add cray-pmi as a Spack external package and add `default-pm=cray` when building LCI with `spack install`.

LCI needs to find the Perlmutter-installed Cray-PMI library. Do `module load cray-pmi` and then run the CMake command to configure LCI. Make sure you see something like this in the output:
```
-- Found PMI: /opt/cray/pe/pmi/6.1.15/lib/libpmi.so  
-- Found PMI2: /opt/cray/pe/pmi/6.1.15/lib/libpmi2.so
```

When building LCI with `spack install`, you need to first add cray-pmi as a Spack external package. Put the following code in `~/.spack/packages.yaml`:
```
  cray-pmi:
      externals:
      - spec: cray-pmi@6.1.15
        modules:
        - cray-pmi/6.1.15
      buildable: false
```
Afterwards, you can use `spack install lci default-pm=cray`.
</details>

### TACC Frontera
<details>
<summary>Click to expand</summary>
tl;dr: use `ibrun` and the MPI bootstrap backend in LCI.

Frontera recommends using its `ibrun` command to launch multi-node applications, and its `ibrun` is tightly coupled with its MPI installation. Therefore, the recommended way to run LCI applications on Frontera is to use the MPI bootstrap backend. You can do this by setting the CMake variable `LCT_PMI_BACKEND_ENABLE_MPI=ON` and linking LCI to the MPI library.

It is also possible to directly use its `srun` and the PMI2 bootstrap backend in LCI. However, we found that this method can result in very slow bootstrapping times for large numbers of ranks (>=512).
</details>

# Write LCI programs

## Overview

LCI is a C++ library that is implemented in C++17 but its public header is compatible with C++11 and later versions. A C API is planned—please open an issue if you need it. The LCI API is structured into four core components:

- Resource Management: Allocation and deallocation of LCI resources.
- Communication Posting: Posting communication operations.
- Completion Checking: Checking the status of posted operations.
- Progress: Ensuring that pending communication can be moved forward.

All LCI APIs are defined in `lci.hpp` and wrapped in the `lci` namespace.

### Objectified Flexible Functions (OFF)

All LCI functions have a _x variant that implements the Objectified Flexible Functions (OFF) idiom. OFF allows optional arguments to be specified in any order, similar to named arguments in Python.

```
# the default post_send function
auto ret = post_send(rank, buf, size, tag, comp);
# the OFF variants
auto ret = post_send_x(rank, buf, size, tag, comp).device(device)();
auto ret = post_send_x(rank, buf, size, tag, comp).matching_policy(matching_policy_t::rank_only).device(device)();
```
Internally, an OFF is implemented as a functor with a constructor for positional arguments and setter methods for optional ones. The () operator triggers the actual function call.

### Resource Management
As a beginner user, you may not need to worry about this part. The only functions you need to know are
```cpp
// Initialize the default LCI runtime.
// Similar to MPI_Init.
lci::g_runtime_init();

// Query the rank and total number of ranks.
// Similar to MPI_Comm_rank(MPI_COMM_WORLD) and MPI_Comm_size(MPI_COMM_WORLD).
int rank_me = lci::g_rank_me();
int rank_n = lci::g_rank_n();

// Finalize the default LCI runtime.
// Similar to MPI_Finalize.
lci::g_runtime_fina();
```

For advanced users, you may want to know the following functions/resources (not a complete list):
##### Runtime
Instead of using the global default runtime, you can create your own runtime. This is useful if you are writing a library that might be used in an application that already uses LCI.
```cpp
// Create a new LCI runtime.
lci::runtime_t runtime = lci::alloc_runtime();
// Free the LCI runtime.
lci::free_runtime(&runtime);
// Example: post a receive operation to a specific runtime.
lci::post_recv_x(rank, buf, size, tag, comp).runtime(runtime)();
```
All LCI OFFs can take a runtime as an optional argument. If not specified, the default runtime will be used.

##### Device
A device represents a set of communication resources. Using separate devices per thread ensures thread isolation:
```cpp
// Create a new LCI device.
lci::device_t device = lci::alloc_device();
// Free the LCI device.
lci::free_device(&device);
// Example: post a receive operation to a specific device.
lci::post_recv_x(rank, buf, size, tag, comp).device(device)();
```

### Communication Posting

LCI supports:
- Send-receive with tag matching.
- Active message.
- RMA (Remote Memory Access): put/get/put with notification.

For those who are not familiar with the terminology: active message is a mechanism that allows the sender to send a message to the receiver and execute a callback function on the receiver side. The receiver does not need to post a receive. RMA is a mechanism that allows one process to read/write the memory of another process without involving the target process.

Below shows the default function signatures of these operations:
```cpp
using namespace lci;

status_t post_send(int rank, void* local_buffer, size_t size, tag_t tag, comp_t local_comp);

status_t post_recv(int rank, void* local_buffer, size_t size, tag_t tag, comp_t local_comp);

status_t post_am(int rank, void* local_buffer, size_t size, comp_t local_comp, rcomp_t remote_comp);

status_t post_put(int rank, void* local_buffer, size_t size, comp_t local_comp, uintptr_t remote_buffer, rmr_t rmr);

status_t post_get(int rank, void* local_buffer, size_t size, comp_t local_comp, uintptr_t remote_buffer, rmr_t rmr);
```

Where is write with notification? `post_put` can take an optional `rcomp` argument to specify a remote completion handle.

All functions here are non-blocking: they merely *post* the communication operation and return immediately. The actual communication may not be completed yet. A communication posting function returns a `status_t` object that can be in one of the three states:
- **done**: the operation is completed.
- **posted**: the operation is posted but not completed yet.
- **retry**: the operation cannot be posted due to a non-fatal error (typically, some resources are temporarily unavailable). You can retry the posting.

You can use the `is_done/is_posted/is_retry` methods to check the status of the operation.

Below is an example of how to post a send operation and blocking wait for it to complete:
```cpp
void post_send(int rank, void* buf, size_t size, tag_t tag) {
  comp_t sync = alloc_sync();
  status_t status;
  do {
    status = post_send(rank, buf, size, tag, sync);
    progress();
  } while (status.is_retry());
  if (status.is_posted()) {
    while (sync_test(sync, &status) == false)
      progress();
  }
  assert(status.is_done());
  // At this point, the send operation is completed
  // and the status object contains the information about the completed operation.
}
```
The usage of synchronizer (`sync`) and `progress` will be explained in the next section.

### Completion Checking

LCI offers several mechanisms to detect when a posted operation completes. These mechanisms are designed to support both synchronous and asynchronous programming styles.

#### Synchronizer

A synchronizer behaves similarly to an MPI request. You can wait for its completion using `sync_wait` or test it with `sync_test`.

```cpp
comp_t sync = alloc_sync();
status_t status = post_send(rank, buf, size, tag, sync);
if (status.is_posted()) {
  while (sync_test(sync, nullptr) == false) {
    progress();  // call progress to ensure the operation can be completed
  }
  // Alternatively, you can use sync_wait
}
free_comp(&sync);
```

Optionally, a synchronizer can accept multiple completion signals before becoming ready. For example,

```cpp
comp_t sync = alloc_sync_x().threshold(2)();

for (...) {
  post_send(..., sync);
  post_recv(..., sync);

  status = sync_wait(sync);
}

free_comp(&sync);
```

#### Completion Queue

Applications that expect a large number of asynchronous operations can use a completion queue. Completed operations are pushed into the queue and can be polled later:

```cpp
comp_t cq = alloc_cq();
// can have many pending operations
status_t status = post_am(..., cq, ...);
// Later or in another thread:
status_t status = cq_pop(cq);
if (status.is_done()) {
  // Process completed operation
  // status contains information about the completed operation
  // such as the rank, tag, and buffer.
}
free_comp(&cq);
```

#### Handler

You can also register a callback handler. This is useful for advanced users who want LCI to directly invoke a function upon completion.

```cpp
void my_handler(status_t status) {
  std::free(status.buf); // free the message buffer
}

comp_t handler = alloc_handler(my_handler);
status_t status = post_send(rank, buf, size, tag, handler);
```

The handler is automatically called once the operation completes locally.

#### Graph (Advanced)

For complex asynchronous flows, LCI also supports `graph_t`, a completion object that represents a Directed Acyclic Graph (DAG) of operations and callbacks. This is similar in spirit to CUDA Graphs and can be used to build efficient non-blocking collectives.

(Check the "Non-blocking Barrier" example for more details.)

### Progress

LCI decouples communication progress from posting and completion operations. Unlike MPI, where progress is implicit, LCI uses an explicit `progress()` call.

This enables users to:

* Call `progress()` periodically in application threads
* Dedicate threads to call `progress()` continuously
* Integrate progress into event loops

##### Basic Usage

```cpp
// On default device
lci::progress();

// With specified device
lci::progress_x().device(device)();
```

Call `progress()` frequently to ensure pending operations complete. In multithreaded programs, you can call `progress()` on a specific device.

## Other Materials

Check the `examples` and `tests` subdirectory for more example code.

Read [this paper](https://arxiv.org/abs/2505.01864) to comprehensively understand LCI interface and runtime design. 

Check out the API documentation for more details.

## Examples

### Hello World
This example shows the LCI runtime lifecycle and the query of rank.

<details>
<summary>Click to expand code</summary>
@include hello_world.cpp
</details>

<details>
<summary>Click to expand example output</summary>
```bash
$ lcrun -n 4 ./lci_hello_world 
Hello world from rank 1 of 4 on <hostname>
Hello world from rank 3 of 4 on <hostname>
Hello world from rank 0 of 4 on <hostname>
Hello world from rank 2 of 4 on <hostname>
```
</details>

### Hello World (Active Message)
This example shows the usages of basic communication operations (active message) and completion mechanisms (synchronizer and handler).

<details>
<summary>Click to expand code</summary>
@include hello_world_am.cpp
</details>

<details>
<summary>Click to expand example output</summary>
```bash
$ lcrun -n 4 ./lci_hello_world_am
Rank 1 received active message from rank 0. Payload: Hello from rank 0
Rank 2 received active message from rank 0. Payload: Hello from rank 0
Rank 3 received active message from rank 0. Payload: Hello from rank 0
Rank 0 received active message from rank 0. Payload: Hello from rank 0
```
</details>

### Distributed Array
This example shows the usage of RMA (Remote Memory Access) operations to implement a distributed array.
<details>
<summary>Click to expand code</summary>
@include darray.cpp
</details>

<details>
<summary>Click to expand example output</summary>
```bash
# This example has no output.
$ lcrun -n 4 ./lci_darray
```
</details>

### Non-blocking Barrier

This examples shows the usage of the completion graph and the send/recv operations to implement a non-blocking barrier.

<details>
<summary>Click to expand code</summary>
@include ibarrier.cpp
</details>

<details>
<summary>Click to expand example output</summary>
```bash
$ lcrun -n 4 ./lci_ibarrier 
rank 1 start barrier
rank 2 start barrier
rank 3 start barrier
rank 0 start barrier
rank 0 end barrier
rank 1 end barrier
rank 1 start barrier
rank 3 end barrier
rank 3 start barrier
rank 0 start barrier
rank 2 end barrier
rank 2 start barrier
rank 2 end barrier
rank 2 start barrier
rank 0 end barrier
rank 3 end barrier
rank 3 start barrier
rank 1 end barrier
rank 1 start barrier
rank 0 start barrier
rank 1 end barrier
rank 0 end barrier
rank 2 end barrier
rank 3 end barrier
```
</details>

### Multithreaded Active Message Ping-pong
This example shows the usages of thread-local devices to speedup the active message communication in a multithreaded environment.

<details>
<summary>Click to expand code</summary>
@include pingpong_am_mt.cpp
</details>

<details>
<summary>Click to expand example output (Run on my laptop. Performance may vary.)</summary>
```bash
$ lcrun -n 4 ./lci_pingpong_am_mt 
pingpong_am_mt: 
Number of threads: 4
Number of messages: 1000
Message size: 8 bytes
Number of ranks: 4
Total time: 0.035286 s
Message rate: 0.283399 mmsg/s
Bandwidth: 2.26719 MB/s
```
</details>

# Run LCI applications

In [Quick Start](@ref quickstart), we have shown you how to run LCI applications using `mpirun` or `srun`. Here, we will discuss the bootstrapping process in more detail.

To successfully bootstrap LCI, the launcher (`srun`, `mpirun`, or `lcrun`) must match the bootstrapping backend used by LCI. Normally, LCI will automatically select the right bootstrapping backend based on the environment so no special configuration is needed. However, if you see your applications were launched as a collection of processes all with rank 0, it means something went wrong.

### Run LCI applications with lcrun

You do not need to do anything special to run LCI applications with `lcrun`. However, `lcrun` is a "toy" launcher that is not as scalable as `srun` or `mpirun`. It is mainly used for testing and debugging purposes.

If you ever encounter a problem with `lcrun`, you can remove the temporary folder `~/.tmp/lct_pmi_file-*` and try again.

### Run LCI applications with srun

LCI is shipped with a copy of the SLURM PMI1 and PMI2 client implementation, so normally you can use `srun` to run LCI applications without any extra configuration. You may need to explicitly enable the pmi2 support by `srun --mpi=pmi2`.

On Cray systems, you may need to load the `cray-pmi` module before building LCI as `srun` on some Cray systems only supports Cray PMI.

### Run LCI applications with mpirun

Because there are many different MPI implementations and there are no standard about how they implement `mpirun`, it is slightly more complicated to run LCI applications with `mpirun`. In such cases, the easiest way is to let LCI use MPI to bootstrap. You just need to set the CMake variable `LCT_PMI_BACKEND_ENABLE_MPI=ON` and link LCI to MPI.

It is possible to directly use the PMI backend with `mpirun`, but you need to find the corresponding PMI client library and link LCI to it. Read the following section for more details.

## More details

### Bootstrapping backends

Specifically, LCI has six different bootstrapping backends:
- `pmi1`: Process Management Interface version 1.
- `pmi2`: Process Management Interface version 2.
- `pmix`: Process Management Interface X.
- `mpi`: Use MPI to bootstrap LCI.
- `file`: LCI-specific bootstrapping backend with a shared file system and `flock`.
- `local`: Just set `rank_me` to 0 and `rank_n` to 1.

`pmi1`, `pmi2`, and `pmix` are the recommended backends to use. They are the same backends used by MPI. The `mpi` backend is a fallback option if you cannot find the PMI client library. The `file` backend is a non-scalable bootstrapping backend mainly for testing and debugging purposes.

By default, the source code of LCI is shipped with a copy of the SLURM PMI1 and PMI2 client implementation, so `pmi1` and `pmi2` are always compiled. `pmix` will be compiled if the CMake configuration of LCI finds the PMIx client library. The `mpi` backend must be explicitly asked for by setting the CMake variable `LCT_PMI_BACKEND_ENABLE_MPI=ON`. The `file` and `local` backend is always compiled.

However, the SLURM PMI1 and PMI2 client implementation is not always the best option. For example, if you are using `mpirun`, you may want to use the PMI client library that comes with your MPI implementation. In this case, you need to find the corresponding PMI client library and link LCI to it. `ldd $(which mpirun)` will show you the PMI client library used by `mpirun`. Normally, MPICH uses `hydra-pmi`; Cray-MPICH uses `cray-pmi`; OpenMPI uses `pmix`. After finding the PMI client library, you can reconfigure LCI with the corresponding PMI client library through the `PMI_ROOT`, `PMI2_ROOT`, or `PMIx_ROOT` environment/cmake variables.

A CMake variable `LCT_PMI_BACKEND_DEFAULT` and an environment variable `LCT_PMI_BACKEND` can be used to set a list of backends to try in order (if they are compiled). The first one that works will be used. The default value is `pmi1,pmi2,pmix,mpi,file,local`.

You can use `export LCT_LOG_LEVEL=info` to monitor the bootstrapping procedure.

### Launchers

`srun` and `mpirun` should use one of the PMI backends (or `mpi` as a last resort). `lcrun` will use the `file` backend.

Depending on the SLURM configuration, `srun` may not enable PMI by default. In this case, you can explicitly enable one of the PMI services by using the `--mpi` option.
`srun --mpi=list -n 1 hostname` will show you the available PMI services. You can confirm whether the PMI service has been enabled with `srun env | grep PMI`.

`mpirun` will use the PMI client library that comes with your MPI implementation. As mentioned above, you need to link LCI to the correct PMI client library.

Sometimes, `lcrun` may hang because of a previous failed run. In this case, you can remove the temporary folder `~/.tmp/lct_pmi_file-*` and try again.

### More about the file backend

The `file` backend allows you to launch multiple LCI processes individually without a launcher. This can significantly ease the debugging process.

For example, you can open two terminal windows and run the following commands in each window:
```bash
export LCT_PMI_BACKEND=file
export LCT_PMI_FILE_NRANKS=2
./lci_program
# or launch it with gdb
gdb ./lci_program
```
This will launch two LCI processes with rank 0 and rank 1.
