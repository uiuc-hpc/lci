#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export ABT_ENV_THREAD_STACKSIZE=8192
# export ABT_ENV_SET_AFFINITY=0

./wait 1 2
./abt_wait 1 2

./wait 1 4
./abt_wait 1 4

./wait 1 8
./abt_wait 1 8

./wait 1 16
./abt_wait 1 16

./wait 1 32
./abt_wait 1 32

./wait 1 64
./abt_wait 1 64

./wait 1 128
./abt_wait 1 128

./wait 1 256
./abt_wait 1 256
