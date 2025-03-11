@page Changelog_1_7_7 Changelog v1.7.7

# Change log for LCI v1.7.7

## Major Changes
- Make LCI more robust with the libfabric/cxi provider.
  - new rendezvous protocol `write`.
  - more assertions.
- Remove sysconf from malloc critical path.
  - Can be a major performance bottleneck on some platform (such as Frontera).
- New LCI network backend: UCX.
- Make LCI_sync_test/wait thread-safe against other LCI_sync_test/wait.

## Breaking Changes

## Other Changes
- Update spack package.py; make LCI_progress thread-safe by default.
- By default, configure cmake to install all lci executables.
- Refactor rendevzous protocol code; add rendezvous protocol selection (LCI_RDV_PROTOCOL=write/writeimm).
- Enable the env var LCI_BACKEND_TRY_LOCK_MODE for ofi backend.
- Let LCT Logger also report hostname.
- Add a field (packet) to LCII_context_t.
- Add sendmc: send medium with completion notification.
- Improve LCT hostname setup.
- Refactor performance counters: "on-the-fly" and "on-the-fly-lw" modes.
- Add(lct): add argument parser to LCT.
- Add(lct_tbarrier): add thread barrier to LCT.
- Add(many2many_random): new example many2many_random.
- Add LCI_VERSION macros.
- Improve LCI cmake config file.
- Add(putmac): put medium with completion.
- Rename LCT log ctx from lci to lct; add log for pmi.
- Improve LCT_Assert: only evaluate the string after asserting failed.
- Remove fflush(stderr).
- Add assertion to pmi.
- Improve(ofi backend): better way to call fi_cq_readerr.
- Make mpi_pt2pt also works for singleton case.
- Add more log outputs to lct pcounter.
- Fix lct_parse_pcounter.py.
- Add try_lock_mode: none, global, global_b.
- Improve PMI: add cmake option to turn off PMI1/PMI2 compilation.

## Fixed Issues
- Fix bugs with LCII_make_key (possibly just a refactoring intead of a bug fixing).
- Fix lct log uninitialized error.
- Fix lci-ucx: link to pthreads.
- Fix LCI_IBV_ENABLE_TD option.
- Fix op undercount and buffer overflow for non-power-of-two (#67).
- Fix cache padding in device.
- Fix spack package pmix option.
- Fix README typo.
- Fix lcit initData: affect lcitb's performance.
- Fix try_lock_mode.