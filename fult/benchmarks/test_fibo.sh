#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export ABT_ENV_THREAD_STACKSIZE=8192
export ABT_ENV_SET_AFFINITY=0

./fibo 4
./abt_fibo 4 1

./fibo 5
./abt_fibo 5 1

./fibo 6
./abt_fibo 6 1

./fibo 7
./abt_fibo 7 1

./fibo 8 
./abt_fibo 8 1

./fibo 9
./abt_fibo 9 1
