#!/bin/bash
#SBATCH --partition=marvin
#SBATCH --time=30:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=test
#SBATCH --output=run/slurm_output.%x-o%j
#SBATCH --error=run/slurm_output.%x-o%j

# exit when any command fails
set -e
# import the the script containing common functions
source ../../include/scripts.sh

# get the LCI source path via environment variable or default value
LCI_SOURCE_PATH=$(realpath "${LCI_SOURCE_PATH:-../../../}")

if [[ -f "${LCI_SOURCE_PATH}/lci/api/lci.h" ]]; then
  echo "Found LCI at ${LCI_SOURCE_PATH}"
else
  echo "Did not find LCI at ${LCI_SOURCE_PATH}!"
  exit 1
fi

if [[ -d "./init/build" ]]; then
  echo "init/build directory already exists! Build LCI on top of previous build."
else
  mkdir -p init
fi
# move to ./init directory
cd init

# setup module environment
module purge
module load gcc
module load cmake
module load openmpi
export CC=gcc
export CXX=g++

# record build status
record_env

mkdir -p log
mv *.log log

# build LCI
mkdir -p build
cd build
echo "Running cmake..."
LCI_INSTALL_PATH=$(realpath "../install")
cmake -DCMAKE_INSTALL_PREFIX=${LCI_INSTALL_PATH} \
      -DCMAKE_BUILD_TYPE=Release \
      -DLCI_OPTIMIZE_FOR_NATIVE=ON \
      -DLCI_USE_DREG=ON \
      -DLCI_DEBUG=OFF \
      -DLCI_SERVER=ibv \
      -DLCI_PM_BACKEND=mpi \
      -DLCIXX_USE_CTEST_EXE=srun \
      -DLCIXX_USE_CTEST_ARGS="--mpi=pmi2" \
      -L \
      ${LCI_SOURCE_PATH} | tee init-cmake.log 2>&1 || { echo "cmake error!"; exit 1; }
cmake -LAH . >> init-cmake.log
echo "Building..."
cmake --build . --parallel | tee init-build.log 2>&1 || { echo "build error!"; exit 1; }
#echo "Installing LCI to ${LCI_INSTALL_PATH}"
#mkdir -p ${LCI_INSTALL_PATH}
#make install > init-install.log 2>&1 || { echo "install error!"; exit 1; }
mv *.log ../log

ctest --extra-verbose --timeout 60
