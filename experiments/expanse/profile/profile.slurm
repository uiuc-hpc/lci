#!/bin/bash
#SBATCH --partition=compute
#SBATCH --account=uic193
#SBATCH --time=5:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=profile
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j

export PATH_TO_BUILD_DIR=${1:-./init/build/}
export PATH_TO_EXE=${PATH_TO_BUILD_DIR}/benchmarks

srun --mpi=pmi2 profile_wrapper.sh