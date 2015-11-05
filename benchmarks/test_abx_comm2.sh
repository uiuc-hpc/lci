#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export ABT_ENV_THREAD_STACKSIZE=16384
export ABT_ENV_SET_AFFINITY=0
export SIZE=$1

echo '---'
mpiexec -n 2 ./comm 1 64 $SIZE
mpiexec -n 2 ./comm3 1 64 $SIZE
mpiexec -n 2 ./abx_comm 1 64 $SIZE
mpiexec -n 2 ./abx_comm2 1 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 4 64 $SIZE
mpiexec -n 2 ./comm3 4 64 $SIZE
mpiexec -n 2 ./abx_comm 4 64 $SIZE
mpiexec -n 2 ./abx_comm2 4 64 $SIZE
