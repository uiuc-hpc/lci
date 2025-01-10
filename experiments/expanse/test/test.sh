#!/bin/bash

# exit when any command fails
set -e
# import the the script containing common functions
source ../../include/scripts.sh

# create the ./init directory
mkdir_s ./init
mkdir_s ./run

for i in $(eval echo {1..${1:-1}}); do
  sbatch test.slurm || { echo "sbatch error!"; exit 1; }
done