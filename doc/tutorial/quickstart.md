@page quickstart Quick Start

This tutorial will show you how to quickly create an LCI project from scratch.

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

## Create a project

Create a new directory for your project:
```bash
mkdir lci_project
cd lci_project
```

Create a `CMakeLists.txt` file in the project directory:
```cmake
cmake_minimum_required(VERSION 3.12)
project(lci_project LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Fetch LCI from GitHub
# This is a CMake feature that allows you to download and build LCI as part of your project.
# You can also use `find_package(LCI)` if you have LCI installed on your system.
include(FetchContent)
FetchContent_Declare(
  lci
  GIT_REPOSITORY https://github.com/uiuc-hpc/lci.git
)
FetchContent_MakeAvailable(lci)

# Add a new executable
add_executable(hello_world hello_world.cpp)
target_link_libraries(hello_world PRIVATE LCI::lci)
```

Create a `hello_world.cpp` file in the project directory:
```cpp
#include <iostream>
#include <unistd.h>
#include "lci.hpp"

int main(int argc, char** args)
{
  char hostname[64];
  gethostname(hostname, 64);
  lci::g_runtime_init();
  std::cout << "Hello world from rank " << lci::get_rank_me() << " of "
            << lci::get_rank_n() << " on " << hostname << std::endl;
  lci::g_runtime_fina();
  return 0;
}
```

Build the project:
```bash
mkdir build
cd build
cmake ..
make
```
This will create an executable called `hello_world` in the `build` directory.

You can run the executable using `lcrun` (on laptop) or `srun` (on cluster):

On your laptop:
```bash
cd /path/to/lci_project/build
# you can copy the `lcrun` script to wherever you want
./_deps/lci-src/lcrun -n 4 ./hello_world
```
`lcrun` is a script that can be used to run LCI applications. It is similar to `mpirun` or `srun`, but not as scalable. It is a good tool for fast testing and debugging.

On a cluster:
```bash
cd /path/to/lci_project/build
# get an interactive node or submit a job with sbatch
srun --mpi=pmi2 -n 4 ./hello_world
```
`--mpi=pmi2` makes sure that `srun` enables its PMI2 support. It is typically the default option for `srun`, so you may not need to specify it.

You should see the following output (the order of the output may vary):
```
Hello world from rank 1 of 4 on <hostname>
Hello world from rank 3 of 4 on <hostname>
Hello world from rank 0 of 4 on <hostname>
Hello world from rank 2 of 4 on <hostname>
```