#!/bin/bash
#SBATCH --partition=compute
#SBATCH --account=uic193
#SBATCH --time=5:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=lt
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j

PATH_TO_BUILD_DIR=${1:-./init/build/}
PATH_TO_EXE=${PATH_TO_BUILD_DIR}/benchmarks

echo "srun mpi"
srun --mpi=pmi2 ${PATH_TO_EXE}/mpi_pt2pt --nthreads 1 --thread-pin 1 --max-msg-size 4194304

echo "srun lci 2s"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2s --nthreads 1 --thread-pin 1 --max-msg-size 8
echo "srun lci 2m"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2m --nthreads 1 --thread-pin 1
echo "srun lci 2m dyn"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2m --nthreads 1 --thread-pin 1 --send-dyn 1 --recv-dyn 1
echo "srun lci 2l"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2l --nthreads 1 --thread-pin 1 --max-msg-size 4194304
echo "srun lci 2l reg"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2l --nthreads 1 --thread-pin 1 --send-reg 1 --recv--reg 1 --max-msg-size 4194304

echo "srun lci 1s"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1s --nthreads 1 --thread-pin 1 --max-msg-size 8
echo "srun lci 1m"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1m --nthreads 1 --thread-pin 1
echo "srun lci 1m dyn"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1m --nthreads 1 --thread-pin 1 --send-dyn 1
echo "srun lci 1l"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1l --nthreads 1 --thread-pin 1 --max-msg-size 4194304
echo "srun lci 1l reg"
srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1l --nthreads 1 --thread-pin 1 --send-reg 1 --max-msg-size 4194304

#echo "srun lci 2s --nthreads 64"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2s --nthreads 64 --thread-pin 1 --max-msg-size 8
#echo "srun lci 2m --nthreads 64"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2m --nthreads 64 --thread-pin 1
#echo "srun lci 2m --nthreads 64 --send-dyn 1 --recv-dyn 1"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2m --nthreads 64 --thread-pin 1 --send-dyn 1 --recv-dyn 1
#echo "srun lci 2l --nthreads 64"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2l --nthreads 64 --thread-pin 1 --max-msg-size 4194304
#echo "srun lci 2l --nthreads 64 --send-reg 1 --recv--reg 1"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 2l --nthreads 64 --thread-pin 1 --send-reg 1 --recv--reg 1 --max-msg-size 4194304
#
#echo "srun lci 1s --nthreads 64"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1s --nthreads 64 --thread-pin 1 --max-msg-size 8
#echo "srun lci 1m --nthreads 64"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1m --nthreads 64 --thread-pin 1
#echo "srun lci 1m --nthreads 64 --send-dyn 1"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1m --nthreads 64 --thread-pin 1 --send-dyn 1
#echo "srun lci 1l --nthreads 64"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1l --nthreads 64 --thread-pin 1 --max-msg-size 4194304
#echo "srun lci 1l --nthreads 64 --send-reg 1"
#srun --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --op 1l --nthreads 64 --thread-pin 1 --send-reg 1 --max-msg-size 4194304
