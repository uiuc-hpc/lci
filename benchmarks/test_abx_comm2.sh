#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export ABT_ENV_THREAD_STACKSIZE=16384
export ABT_ENV_SET_AFFINITY=0
export MV2_ENABLE_AFFINITY=0
export SIZE=$1

set -x

echo '---'
mpiexec -n 2 -ppn 1 ./comm 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm 10 64 $SIZE

mpiexec -n 2 -ppn 1 ./comm3 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 10 64 $SIZE

mpiexec -n 2 -ppn 1 ./abx_comm 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 10 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 10 64 $SIZE

#echo '---'
#mpiexec -n 2 -ppn 1 ./comm 10 64 $SIZE
#mpiexec -n 2 -ppn 1 ./comm3 10 64 $SIZE
#mpiexec -n 2 -ppn 1 ./abx_comm 10 64 $SIZE

#echo '---'
#mpiexec -n 2 -ppn 1 ./comm 10 64 $SIZE
#mpiexec -n 2 -ppn 1 ./comm3 10 64 $SIZE
#mpiexec -n 2 -ppn 1 ./abx_comm 10 64 $SIZE
