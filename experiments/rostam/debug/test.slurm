#!/bin/bash
#SBATCH --partition=marvin
#SBATCH --time=30:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=test
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j

PATH_TO_EXE=${1:-./init/build/}

cd ${PATH_TO_EXE}
ctest --extra-verbose --timeout 60
