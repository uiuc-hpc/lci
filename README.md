# Lightweight Communication Interface (LCI)
Implementation of a cool communication layer.

## Authors

- \@danghvu (danghvu@gmail.com)
- \@omor1
- \@JiakunYan (jiakunyan1998@gmail.com)
- \@snirmarc

## Installation
```
cmake .
make
make install
```

### Important CMake variables
- `CMAKE_INSTALL_PREFIX=/path/to/install`: Where to install LCI
  - This is the same across all the cmake projects.
- `LCI_SERVER=ibv/ofi`: The network backend to use.
  - ibv: libibverbs, typically for infiniband.
  - ofi: libfabrics, for all other networks (slingshot-11, ethernet, shared memory).
- `LCI_PM_BACKEND_DEFAULT=pmi1/pmi2/pmix/mpi`: The default PMI backend to use.
  - Refer to [Choose the right PMI backend](#choose-the-right-pmi-backend) 
    for which backend you should use.
- `LCI_DEBUG=ON/OFF`: Enable/disable the debug mode (more assertions and log).

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

### Choose the right PMI backend
Choosing the right PMI backend to use is important. LCI offers four PMI backends: pmi1,
pmi2, pmix, and mpi. You can use `LCI_PM_BACKEND_DEFAULT` to specify the default
backend when running `cmake`, or the environment variable `LCI_PM_BACKEND` to specify
the backend when executing the program.

An easy way to decide the right PMI backend is to run
```
mpirun -n 1 env | grep PMI
```
if you are using `mpirun`, or
```
srun -n 1 env | grep PMI
```
if you are using `srun`. If you see `PMI_RANK` in the output, you should use either pmi1
or pmi2. If you see `PMIX_RANK` in the output, you should use pmix.

If you are using `srun` but can see neither `PMI_RANK` nor `PMIX_RANK`, you should try
the `--mpi` arguments of `srun`. For example,
```
srun --mpi=pmi2 -n 1 ./application
```
should set up a pmi2 environment.
```
srun --mpi=pmix -n 1 ./application
```
should set up a pmix environment.

As a last resort, you can always use the `mpi` backend.

## Write a LCI program

See `examples` and `tests` for some example code.

See `src/api/lci.h` for public APIs.

`doxygen` for a full [documentation](https://uiuc-hpc.github.io/LC/).

## LICENSE
See LICENSE file.
