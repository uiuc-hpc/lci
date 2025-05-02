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

# Write LCI programs

See `examples` and `tests` for some example code.

Read [this paper](https://jiakunyan.github.io/source/lci_preprint.pdf) to comprehensively understand LCI interface and runtime design. 

<!-- TODO: Add more examples and tests. -->

Check out the API documentation for more details.

# Run LCI applications

In [Quick Start](@ref quickstart), we have shown you how to run LCI applications using `mpirun` or `srun`. Here, we will discuss the bootstrapping process in more detail.

To successfully bootstrap LCI, the launcher (`srun`, `mpirun`, or `lcrun`) must match the bootstrapping backend used by LCI. Normally, LCI will automatically select the right bootstrapping backend based on the environment so no special configuration is needed. However, if you see your applications were launched as a collection of processes all with rank 0, it means something went wrong.

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
