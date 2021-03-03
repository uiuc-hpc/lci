add_lci_tests(
        TESTS
            init.c
            pt2pti.c
            pt2ptiq.c
            pt2ptbc.c
            pt2ptd.c
            puti.c
            putb.c
            putbc.c
            pt2ptbc_mt.cpp
        LABELS pingpong
        COMMAND "${SRUN_EXE} ${SRUN_ARG} [TARGET]"
)