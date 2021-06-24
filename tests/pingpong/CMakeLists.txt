add_lci_tests(
        TESTS
            init.c
            pt2pts.c
            pt2ptsq.c
            pt2ptm.c
            pt2ptmn.c
            pt2ptl.c
            puts.c
            putma.c
            putmna.c
            putla.c
            pt2ptm_mt.cpp
        LABELS pingpong
        COMMAND "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET]"
)