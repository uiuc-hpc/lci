#!/bin/bash

# exit when any command fails
set -e
# import the the script containing common functions
source ../../include/scripts.sh

TASKS=("lt" "bw")

mkdir_s ./data
mkdir_s ./draw

for task in "${TASKS[@]}"; do
  python parse_${task}.py || { echo "sbatch error!"; exit 1; }
  python draw_${task}.py || { echo "sbatch error!"; exit 1; }
done