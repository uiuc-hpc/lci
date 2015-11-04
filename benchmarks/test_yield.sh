#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export ABT_ENV_THREAD_STACKSIZE=8192
#export ABT_ENV_SET_AFFINITY=0

./yield 1 2
./abt_yield 1 2

./yield 1 4
./abt_yield 1 4

./yield 1 8
./abt_yield 1 8

./yield 1 16
./abt_yield 1 16

./yield 1 32
./abt_yield 1 32

./yield 1 64
./abt_yield 1 64

./yield 1 128
./abt_yield 1 128

./yield 1 256
./abt_yield 1 256

./yield 1 512
./abt_yield 1 512


