add_lci_tests(
        TESTS
        pt2pti.c
        pt2ptm.c
        pt2ptl.c
            puti-self.c
        pt2ptmn.c
            putbc-self.c
        LABELS loopback
        COMMAND "[TARGET]"
)