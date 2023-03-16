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
- `LCI_DEBUG=ON/OFF`: Enable/disable the debug mode (more assertions and logs).
  The default value is `OFF`.
- `LCI_SERVER=ibv/ofi`: Hint to which network backend to use. If both `ibv` and `ofi` are found, LCI will use the one
  indicated by this variable. The default value is `ibv`. Typically, you don't need to
  modify this variable as if `libibverbs` presents, it is likely to be the recommended one to use.
  - `ibv`: libibverbs, typically for infiniband.
  - `ofi`: libfabrics, for all other networks (slingshot-11, ethernet, shared memory).
- `LCI_FORCE_SERVER=ON/OFF`: Default value is `OFF`. If it is set to `ON`, 
  `LCI_SERVER` will not be treated as a hint but a requirement.

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

## Write an LCI program

See `examples` and `tests` for some example code.

See `src/api/lci.h` for public APIs.

`doxygen` for a full [documentation](https://uiuc-hpc.github.io/LC/).

## LICENSE
See LICENSE file.
