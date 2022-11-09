add_lci_tests(
  TESTS
  lcit_pt2pt.cpp
  LABELS
  lcit.medium
  COMMANDS
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2s --nthreads 4"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2m --nthreads 4"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2m --nthreads 4 --send-dyn 1 --recv-dyn 1"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2l --nthreads 4"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2l --nthreads 4 --send-reg 1 --recv--reg 1"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2l --nthreads 4 --recv-dyn 1"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 1s --nthreads 4"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 1m --nthreads 4"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 1l --nthreads 4")

add_lci_tests(
  TESTS
  lcit_pt2pt.cpp
  LABELS
  lcit.small
  COMMANDS
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2s --nthreads 4 --nsteps 10"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2m --nthreads 4 --nsteps 10"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2m --nthreads 4 --send-dyn 1 --recv-dyn 1 --nsteps 10"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2l --nthreads 4 --nsteps 10"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2l --nthreads 4 --send-reg 1 --recv--reg 1 --nsteps 10"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 2l --nthreads 4 --recv-dyn 1 --nsteps 10"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 1s --nthreads 4 --nsteps 10"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 1m --nthreads 4 --nsteps 10"
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET] --op 1l --nthreads 4 --nsteps 10"
)

add_lci_tests(
  TESTS
  lcit_many2one.cpp
  LABELS
  lcit.small
  COMMANDS
  "${SRUN_EXE} -n 3 ${SRUN_EXTRA_ARG} [TARGET] --op 2m --nthreads 3 --send-window 1 --recv-window 100 --nsteps 10"
  "${SRUN_EXE} -n 3 ${SRUN_EXTRA_ARG} [TARGET] --op 2l --nthreads 3 --send-window 1 --recv-window 100 --nsteps 10"
  "${SRUN_EXE} -n 3 ${SRUN_EXTRA_ARG} [TARGET] --op 1m --nthreads 3 --send-window 1 --recv-window 100 --nsteps 10"
  "${SRUN_EXE} -n 3 ${SRUN_EXTRA_ARG} [TARGET] --op 1l --nthreads 3 --send-window 1 --recv-window 100 --nsteps 10"
)
