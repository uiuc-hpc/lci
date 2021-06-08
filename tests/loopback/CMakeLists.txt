add_lci_tests(
        TESTS
            pt2pts.c
            pt2ptm.c
            pt2ptm_mt.cpp
            pt2ptl.c
            pt2ptmn.c
            putla.c
#            puti-self.c
#            putbc-self.c
        LABELS loopback
        COMMAND "[TARGET]"
)