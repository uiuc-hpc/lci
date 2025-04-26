@page Changelog_1_7_6 Changelog v1.7.6

# Change log for LCI v1.7.6

## Major Changes
- Split the original C library, LCI, into two libraries: a C library, LCI, 
  and a C++ library, LCT (Lightweight Communication Tools).
  - LCT provides basic tools that can be used across libraries, including
    - timing
    - string searching and manipulation
    - query thread ID and number
    - logging
    - performance counters
    - different implementation of queues
    - PMI (Process Management Interface) wrappers
  - The CMake variable `LCI_WITH_LCT_ONLY` can be used to enable LCT-only build.
- Add(`LCI_cq_createx`): be able to specify the cq max length.
  - This is a temporary workaround for multiple devices.
- Add(`LCI_ENABLE_PRG_NET_ENDPOINT`): control whether to use the progress-specific network endpoint

## Breaking Changes
- The lib output name is changed from LCI/LCT to lci/lct.
- Change `LCI_IBV_ENABLE_TRY_LOCK_QP` to `LCI_IBV_ENABLE_TD` and make it an env var.

## Other Changes
- Change the default endpoint number from 8 to 1024.
- Change the CircleCI config to Debug build; Enable performance counter in debug CI.
- Merge lcii_config.h.in into lci_config.h.in.
- Improve(pcounter): do not call LCT_now when the performance counters are not enabled.
- lci-ucx rcache: turn off assertion by default

## Fixed Issues
- Fix liblci.pc.in.
- Fix the ibv backend with old libibverbs that does not support odp.