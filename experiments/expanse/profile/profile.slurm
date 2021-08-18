#!/bin/bash
#SBATCH --partition=compute
#SBATCH --account=uic193
#SBATCH --time=5:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=profile
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j

PATH_TO_BUILD_DIR=${1:-./init/build/}
PATH_TO_EXE=${PATH_TO_BUILD_DIR}/benchmarks

mkdir -p result

srun --mpi=pmi2 perf record --freq=997 --call-graph dwarf -q -o result/perf.data ${PATH_TO_EXE}/lcitb_pt2pt --op 2m --nthreads 16 --thread-pin 1