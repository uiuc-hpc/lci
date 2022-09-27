#!/bin/bash

# exit when any command fails
set -e
# import the the script containing common functions
source ../../include/scripts.sh

sbatch_path=$(realpath "${sbatch_path:-.}")
build_path=$(realpath "${exe_path:-init/build/}")

if [[ -d "${build_path}" ]]; then
  echo "Run LCI profile at ${exe_path}"
else
  echo "Did not find profile at ${exe_path}!"
  exit 1
fi

# create the ./run directory
mkdir_s ./profile
cd profile

sbatch ${sbatch_path}/profile.slurm ${build_path} || { echo "sbatch error!"; exit 1; }
cd ..