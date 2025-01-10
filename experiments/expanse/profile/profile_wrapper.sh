#!/bin/bash

echo "perf record --freq=99 --call-graph dwarf -q -o perf.data.$SLURM_JOB_ID.$SLURM_PROCID ${ROOT_PATH}/init/build/benchmarks/lcitb_pt2pt --op 2m --send-comp-type=sync --recv-comp-type=sync --nthreads 64 --thread-pin 1 --nsteps=1000 --min-msg-size=2048 --max-msg-size=2048"
perf record --freq=99 --call-graph dwarf -q -o perf.data.$SLURM_JOB_ID.$SLURM_PROCID ${ROOT_PATH}/init/build/benchmarks/lcitb_pt2pt --op 2m --send-comp-type=sync --recv-comp-type=sync --nthreads 64 --thread-pin 1 --nsteps=100 --min-msg-size=2048 --max-msg-size=2048 --send-window 64