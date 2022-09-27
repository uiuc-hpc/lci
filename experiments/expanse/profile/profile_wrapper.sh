#!/bin/bash

echo "perf record --freq=997 --call-graph dwarf -q -o perf.data.$SLURM_JOB_ID ${PATH_TO_EXE}/lcitb_pt2pt --op 2l --send-comp-type=sync --recv-comp-type=sync --nthreads 16 --thread-pin 1 --nsteps=100000 --max-msg-size=8"
perf record --freq=997 --call-graph dwarf -q -o perf.data.$SLURM_JOB_ID ${PATH_TO_EXE}/lcitb_pt2pt --op 2l --send-comp-type=sync --recv-comp-type=sync --nthreads 16 --thread-pin 1 --nsteps=100000 --max-msg-size=8