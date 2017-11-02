# LCI
Implementation of a cool communication layer.

## Authors

- \@danghvu (danghvu@gmail.com)
- \@snirmarc

## Installation
```
make
make install PREFIX=$INSTALLDIR
```

## Hack

See [examples/](https://github.com/uiuc-hpc/LC/tree/master/examples) for some example code.

See [include/lc.h](https://github.com/uiuc-hpc/LC/blob/master/include/lc.h) for public APIs.

`doxygen` for a full documentation.

## MPI Interoperation

We use PMI for launching the jobs and hook to MPI rank. Since we have no
control on what MPI may do with the PMI, initialize with LCI first (lc_open)
before MPI_Init is the best approach.

## LICENSE
TBD
