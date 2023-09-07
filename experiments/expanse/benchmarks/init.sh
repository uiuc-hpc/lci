#!/usr/bin/bash

# exit when any command fails
set -e
# import the the script containing common functions
source ../../include/scripts.sh

# get the ibvBench source path via environment variable or default value
LCI_SOURCE_PATH=$(realpath "${LCI_SOURCE_PATH:-../../../}")

if [[ -f "${LCI_SOURCE_PATH}/lci/api/lci.h" ]]; then
  echo "Found LCI at ${LCI_SOURCE_PATH}"
else
  echo "Did not find LCI at ${LCI_SOURCE_PATH}!"
  exit 1
fi

# create the ./init directory
mkdir_s ./init
# move to ./init directory
cd init

# setup module environment
module purge
module load DefaultModules
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
      -DLCI_DEBUG=OFF \
      -DLCI_SERVER=ibv \
      -DLCI_PM_BACKEND=pmi1 \
      -DSRUN_EXE=srun \
      -DSRUN_EXTRA_ARG="--mpi=pmi2" \
      -DLCI_PACKET_SIZE_DEFAULT=69632 \
      -L \
      ${LCI_SOURCE_PATH} | tee init-cmake.log 2>&1 || { echo "cmake error!"; exit 1; }
cmake -LAH . >> init-cmake.log
echo "Running make..."
make VERBOSE=1 -j | tee init-make.log 2>&1 || { echo "make error!"; exit 1; }
#echo "Installing taskFlow to ${LCI_INSTALL_PATH}"
#mkdir -p ${LCI_INSTALL_PATH}
#make install > init-install.log 2>&1 || { echo "install error!"; exit 1; }
mv *.log ../log