add_lci_tests(
  TESTS
  init.cpp
  # pt2pts.c
  # pt2ptsq.c
  # pt2ptm.c
  # pt2ptm_wildcast.c
  # pt2ptmn.c
  # pt2ptl.c
  # pt2ptl_dyn.c
  # puts.c
  # putma.c
  # putmna.c
  # putla.c
  # putla_dyn.c
  # putva.c
  # pt2ptm_mt.cpp
  LABELS
  pingpong
  COMMANDS
  "${LCIXX_USE_CTEST_EXE} -n 2 ${LCIXX_USE_CTEST_ARGS} [TARGET]"
  DEPENDENCIES
  OpenMP::OpenMP_CXX)
