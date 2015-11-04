#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export ABT_ENV_THREAD_STACKSIZE=16384
export ABT_ENV_SET_AFFINITY=0
export SIZE=$1

echo '---'
mpiexec -n 2 ./comm_order 1 64 $SIZE
mpiexec -n 2 ./abx_comm_order 1 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 1 64 $SIZE

echo '---'
mpiexec -n 2 ./comm_order 4 64 $SIZE
mpiexec -n 2 ./abx_comm_order 1 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 4 64 $SIZE

echo '---'
mpiexec -n 2 ./comm_order 8 64 $SIZE
mpiexec -n 2 ./abx_comm_order 8 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 8 64 $SIZE

echo '---'
mpiexec -n 2 ./comm_order 12 64 $SIZE
mpiexec -n 2 ./abx_comm_order 12 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 12 64 $SIZE

echo '---'
mpiexec -n 2 ./comm_order 16 64 $SIZE
mpiexec -n 2 ./abx_comm_order 16 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 16 64 $SIZE

echo '---'
mpiexec -n 2 ./comm_order 20 64 $SIZE
mpiexec -n 2 ./abx_comm_order 20 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 20 64 $SIZE

echo '---'
mpiexec -n 2 ./comm_order 24 64 $SIZE
mpiexec -n 2 ./abx_comm_order 24 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 24 64 $SIZE

echo '---'
mpiexec -n 2 ./comm_order 28 64 $SIZE
mpiexec -n 2 ./abx_comm_order 28 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 28 64 $SIZE

echo '---'
mpiexec -n 2 ./comm_order 32 64 $SIZE
mpiexec -n 2 ./abx_comm_order 32 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 32 64 $SIZE

echo '---'
mpiexec -n 2 ./comm_order 36 64 $SIZE
mpiexec -n 2 ./abx_comm_order 36 64 $SIZE
mpiexec -n 2 ./abx_comm2_order 36 64 $SIZE
