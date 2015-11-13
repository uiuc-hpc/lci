#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export ABT_ENV_THREAD_STACKSIZE=16384
export ABT_ENV_SET_AFFINITY=0
export MV2_ENABLE_AFFINITY=0
export MPICH_ASYNC_PROGRESS=0
export SIZE=$1

set -x

echo '--- 4 64 ---'
#mpiexec -n 2 -ppn 1 ./comm 4 64 $SIZE
#mpiexec -n 2 -ppn 1 ./comm 4 64 $SIZE
#mpiexec -n 2 -ppn 1 ./comm 4 64 $SIZE
#mpiexec -n 2 -ppn 1 ./comm 4 64 $SIZE
#mpiexec -n 2 -ppn 1 ./comm 4 64 $SIZE

mpiexec -n 2 -ppn 1 ./comm3 4 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 4 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 4 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 4 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 4 64 $SIZE

mpiexec -n 2 -ppn 1 ./abx_comm 4 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 4 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 4 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 4 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 4 64 $SIZE

echo '--- 8 64 ---'

mpiexec -n 2 -ppn 1 ./comm3 8 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 8 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 8 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 8 64 $SIZE
mpiexec -n 2 -ppn 1 ./comm3 8 64 $SIZE

mpiexec -n 2 -ppn 1 ./abx_comm 8 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 8 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 8 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 8 64 $SIZE
mpiexec -n 2 -ppn 1 ./abx_comm 8 64 $SIZE

#echo '---'
#mpiexec -n 2 -ppn 1 ./comm 4 64 $SIZE
#mpiexec -n 2 -ppn 1 ./comm3 4 64 $SIZE
#mpiexec -n 2 -ppn 1 ./abx_comm 4 64 $SIZE

#echo '---'
#mpiexec -n 2 -ppn 1 ./comm 4 64 $SIZE
#mpiexec -n 2 -ppn 1 ./comm3 4 64 $SIZE
#mpiexec -n 2 -ppn 1 ./abx_comm 4 64 $SIZE
