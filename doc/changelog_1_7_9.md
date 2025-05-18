# Change log for LCI v1.7.9

## Major Changes
- lct: add external MPMC queue implementation
- add new PMI backend `file` and lcrun
- remove link dependency to network backends in LCIConfig.cmake.in

## Breaking Changes
- rename SRUN_EXE -> LCI_USE_CTEST_EXE

## Other Changes
- packet pool: add option to control global/per-device packet pool (LCI_USE_GLOBAL_PACKET_POOL)
- add options for device lock mode
- improve LCI_INIT_ATTACH_DEBUGGER: make the variable i volatile
- improve various warning messages
- fix macOS build
- cmake: add LCI::LCI LCI::lci as an alias for LCI for autofetch purpose
- improve support for fetchcontent