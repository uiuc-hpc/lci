add_lci_tests(
        TESTS
        lcit_pt2pt.cpp
        lcit_many2one.cpp
        LABELS lcit
        COMMAND "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET]"
)