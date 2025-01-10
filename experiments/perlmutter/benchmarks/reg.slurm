#!/bin/bash
#SBATCH --partition=shared
#SBATCH --account=uic193
#SBATCH --time=5:00
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=reg
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j

PATH_TO_BUILD_DIR=${1:-./init/build/}
PATH_TO_EXE=${PATH_TO_BUILD_DIR}/benchmarks

srun --mpi=pmi2 ${PATH_TO_EXE}/mem_reg_lat 4096 524288 1000