add_lci_tests(
  TESTS
  lcit_pt2pt.cpp
  LABELS
  lcit.medium
  COMMANDS
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2s --nthreads 4"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2m --nthreads 4"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2m --nthreads 4 --send-dyn 1 --recv-dyn 1"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2l --nthreads 4"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2l --nthreads 4 --send-reg 1 --recv--reg 1"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2l --nthreads 4 --recv-dyn 1"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 1s --nthreads 4"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 1m --nthreads 4"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 1l --nthreads 4"
)

add_lci_tests(
  TESTS
  lcit_pt2pt.cpp
  LABELS
  lcit.small
  COMMANDS
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2s --nthreads 4 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2m --nthreads 4 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2m --nthreads 4 --send-dyn 1 --recv-dyn 1 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2m --nthreads 4 --send-window 20 --recv-window 20 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2l --nthreads 4 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2l --nthreads 4 --send-reg 1 --recv--reg 1 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2l --nthreads 4 --recv-dyn 1 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 1s --nthreads 4 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 1m --nthreads 4 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --op 1l --nthreads 4 --nsteps 10"
)

add_lci_tests(
  TESTS
  lcit_many2one.cpp
  LABELS
  lcit.small
  COMMANDS
  "${LCI_USE_CTEST_EXE} -n 3 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2m --nthreads 3 --send-window 1 --recv-window 100 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 3 ${LCI_USE_CTEST_ARGS} [TARGET] --op 2l --nthreads 3 --send-window 1 --recv-window 100 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 3 ${LCI_USE_CTEST_ARGS} [TARGET] --op 1m --nthreads 3 --send-window 1 --recv-window 100 --nsteps 10"
  "${LCI_USE_CTEST_EXE} -n 3 ${LCI_USE_CTEST_ARGS} [TARGET] --op 1l --nthreads 3 --send-window 1 --recv-window 100 --nsteps 10"
)

add_lci_tests(
  TESTS
  lcit_pt2pt.cpp
  LABELS
  lcit.bigmsg
  COMMANDS
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} env LCI_MAX_SINGLE_MESSAGE_SIZE=1002 [TARGET] --op 2l --min-msg-size=1001 --max-msg-size=4004 --nsteps 1"
  "${LCI_USE_CTEST_EXE} -n 2 ${LCI_USE_CTEST_ARGS} env LCI_MAX_SINGLE_MESSAGE_SIZE=1002 [TARGET] --op 1l --min-msg-size=1001 --max-msg-size=4004 --nsteps 1"
)
