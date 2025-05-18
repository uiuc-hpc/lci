add_lci_tests(
  TESTS
  pt2pts.c
  pt2ptm.c
  pt2ptm_mt.cpp
  pt2ptl.c
  pt2ptl_dyn.c
  pt2ptmn.c
  putla.c
  putva.c
  # puti-self.c putbc-self.c
  LABELS
  loopback
  COMMANDS
  "${LCI_USE_CTEST_EXE} -n 1 ${LCI_USE_CTEST_ARGS} [TARGET]"
  DEPENDENCIES
  OpenMP::OpenMP_CXX)
