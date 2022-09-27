#!/bin/bash

perf record --freq=997 --call-graph dwarf -q -o perf.data.$SLURM_PROCID ${PATH_TO_EXE}/lcitb_pt2pt --op 2m --nthreads 16 --thread-pin 1