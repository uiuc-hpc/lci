#!/bin/bash

# exit when any command fails
set -e
# import the the script containing common functions
source ../../include/scripts.sh

TASKS=("mt.slurm")
sbatch_path=$(realpath "${sbatch_path:-.}")
build_path=$(realpath "${exe_path:-init/build/}")

if [[ -d "${build_path}" ]]; then
  echo "Run LCI benchmarks at ${exe_path}"
else
  echo "Did not find benchmarks at ${exe_path}!"
  exit 1
fi

# create the ./run directory
mkdir_s ./run
cd run

for i in $(eval echo {1..${1:-1}}); do
  for task in "${TASKS[@]}"; do
    sbatch ${sbatch_path}/${task} ${build_path} || { echo "sbatch error!"; exit 1; }
  done
done
cd ..