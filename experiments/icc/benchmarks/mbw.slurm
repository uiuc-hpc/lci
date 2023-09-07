#!/bin/bash
#SBATCH --partition=cs
#SBATCH --time=15:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=32
#SBATCH --job-name=mbw
#SBATCH --output=slurm_output.%x-o%j
#SBATCH --error=slurm_error.%x-o%j

PATH_TO_BUILD_DIR=${1:-./init/build/}
PATH_TO_EXE=${PATH_TO_BUILD_DIR}/benchmarks

export LCI_PACKET_RETURN_THRESHOLD=0

echo "srun mbw MPI"
srun -n 2 ${PATH_TO_EXE}/mpi_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --nthreads 1 --thread-pin 1
echo "srun mbw MPI everywhere 64"
srun -n ${SLURM_NTASKS} ${PATH_TO_EXE}/mpi_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --nthreads 1
echo "srun mbw MPI multithread ${SLURM_TASKS_PER_NODE}"
srun -n 2 ${PATH_TO_EXE}/mpi_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1

echo "srun mbw LCI 2s multithread ${SLURM_TASKS_PER_NODE}"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 2s --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync --recv-comp-type=sync --max-msg-size 8
echo "srun mbw LCI 2m multithread ${SLURM_TASKS_PER_NODE}"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 2m --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync --recv-comp-type=sync
echo "srun mbw LCI 2m multithread ${SLURM_TASKS_PER_NODE} dyn"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 2m --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync --recv-comp-type=sync --send-dyn 1 --recv-dyn 1
echo "srun mbw LCI 2l multithread ${SLURM_TASKS_PER_NODE}"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 2l --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync --recv-comp-type=sync
echo "srun mbw LCI 2l multithread ${SLURM_TASKS_PER_NODE} pre-reg"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 2l --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync --recv-comp-type=sync --send-reg 1 --recv-reg 1

echo "srun mbw LCI 1s multithread ${SLURM_TASKS_PER_NODE}"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 1s --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync --max-msg-size 8
echo "srun mbw LCI 1m multithread ${SLURM_TASKS_PER_NODE}"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 1m --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync
echo "srun mbw LCI 1m multithread ${SLURM_TASKS_PER_NODE} dyn"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 1m --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync --send-dyn 1
echo "srun mbw LCI 1l multithread ${SLURM_TASKS_PER_NODE}"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 1l --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync
echo "srun mbw LCI 1l multithread ${SLURM_TASKS_PER_NODE} pre-reg"
srun -n 2 --mpi=pmi2 ${PATH_TO_EXE}/lcitb_pt2pt --send-window 64 --max-msg-size 8192 --nsteps 100 --op 1l --nthreads ${SLURM_TASKS_PER_NODE} --thread-pin 1 --send-comp-type=sync --send-reg 1
