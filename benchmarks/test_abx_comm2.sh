#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export ABT_ENV_THREAD_STACKSIZE=16384
export ABT_ENV_SET_AFFINITY=0
export SIZE=$1

echo '---'
mpiexec -n 2 ./comm 1 64 $SIZE
mpiexec -n 2 ./abx_comm 1 64 $SIZE
mpiexec -n 2 ./abx_comm2 1 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 4 64 $SIZE
mpiexec -n 2 ./abx_comm 1 64 $SIZE
mpiexec -n 2 ./abx_comm2 4 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 8 64 $SIZE
mpiexec -n 2 ./abx_comm 8 64 $SIZE
mpiexec -n 2 ./abx_comm2 8 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 12 64 $SIZE
mpiexec -n 2 ./abx_comm 12 64 $SIZE
mpiexec -n 2 ./abx_comm2 12 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 16 64 $SIZE
mpiexec -n 2 ./abx_comm 16 64 $SIZE
mpiexec -n 2 ./abx_comm2 16 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 20 64 $SIZE
mpiexec -n 2 ./abx_comm 20 64 $SIZE
mpiexec -n 2 ./abx_comm2 20 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 24 64 $SIZE
mpiexec -n 2 ./abx_comm 24 64 $SIZE
mpiexec -n 2 ./abx_comm2 24 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 28 64 $SIZE
mpiexec -n 2 ./abx_comm 28 64 $SIZE
mpiexec -n 2 ./abx_comm2 28 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 32 64 $SIZE
mpiexec -n 2 ./abx_comm 32 64 $SIZE
mpiexec -n 2 ./abx_comm2 32 64 $SIZE

echo '---'
mpiexec -n 2 ./comm 36 64 $SIZE
mpiexec -n 2 ./abx_comm 36 64 $SIZE
mpiexec -n 2 ./abx_comm2 36 64 $SIZE
