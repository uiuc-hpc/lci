@page Changelog_1_7_4 Changelog v1.7.4

# Change log for LCI v1.7.4

## Major Changes
- Greatly improve the ease-of-use of LCI.
  - Automatically find the available PMI backend to use.
  - Automatically find the available network backend to use.
  - Automatically detect whether the libfabric/cxi provider is available.

## Breaking Changes

## Other Changes

## Fixed Issues
- Fix zero-byte messages send when using ibv mlx4 backend.
- Fix the LCI_PM_BACKEND_ENABLE_MPI/PMIX cmake variables.