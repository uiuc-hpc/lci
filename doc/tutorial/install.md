@page install Install LCI

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