add_lci_tests(
        TESTS
            init.c
            pt2pts.c
            pt2ptsq.c
            pt2ptm.c
            pt2ptmn.c
            pt2ptl.c
            puts.c
#            puti.c
#            putb.c
#            putbc.c
            pt2ptm_mt.cpp
        LABELS pingpong
        COMMAND "${SRUN_EXE} ${SRUN_ARG} [TARGET]"
)