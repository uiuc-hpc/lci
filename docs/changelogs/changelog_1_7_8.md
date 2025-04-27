@page Changelog_1_7_8 Changelog v1.7.8

# Change log for LCI v1.7.8

## Major Changes
- Upstream LCI spack package to the spack repo.
- Add RoCE support
  - including the GID auto selection feature.
- Improve multi-device support
  - use shared low-level data structures.
  - use shared heap and package pools for devices
- Fix messages of size larger than int_max.
  - LCI will break the message into multiple chunks and send them with RDMA write.

## Breaking Changes
- Change the github repo name from uiuc-hpc/LC to uiuc-hpc/lci
- Remove the LCI::Shared cmake target

## Other Changes
- improve spack package.py: use spec.satisfies instead of spec.value
- fix(ofi): pass LCI_SERVER_MAX_SENDS/RECVS to libfabric endpoints
- add three performance counters: net_poll_cq_num, sync_stay_timer, cq_stay_timer
- refactor backend: LCI device as LCIS endpoint
- force ofi cxi to disable LCI_ENABLE_PRG_NET_ENDPOINT
- initialize allocated packets with LCII_POOLID_LOCAL
- add net_cq_poll related counters to measure the thread contention in progress engine
- fix(lct/pcounter): exit recording thread if not needed
- ibv backend: adjust max_send, max_recv, max_cqe according to device_attr
- improve LCIConfig.cmake.in: PACKAGE_CMAKE_INSTALL_XXX can be changed by find_dependency
