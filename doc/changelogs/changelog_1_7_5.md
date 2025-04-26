@page Changelog_1_7_5 Changelog v1.7.5

# Change log for LCI v1.7.5

## Major Changes
- Runtime: use buffer directly for inline sendm/putm
  - sendm and putm consume LCI packets even when the send can be inlined.
    This allows such "short" medium sends to bypass normal packet allocation
    and use the user's buffer directly.
- Add LCIX collective communication operations.
- Add LCI_ENABLE_MULTITHREAD_PROGRESS option: Make LCI_progress thread-safe.
- Improve Matching Table: 
  - Improve the original matching table algorithm.
  - Providing two more matching table backends.
  - Refactor the code to enable user select the matching table at runtime.
    - queue
    - hash
    - hashqueue

## Breaking Changes

## Other Changes
- Use check_c_compiler_flag to set LCI_USE_AVX and LCI_OPTIMIZE_FOR_NATIVE.
- spack: add spack package for new LCI version.
- Enable debug build for detecting double free of packets.
- More debugging log.
- Implement LCI_barrier with LCI_send/recv.
- Improve ibv device selection procedure: automatically select the device and
  port with the best performance.
- Modernize LCI: Use C11 stdatomic for all synchronization variables.
- Add an assertion in LCI_putva in case there are too many lbuffers.
- Set the default value of LCI_USE_AVX to be OFF.
- improve(cmake): Only set LCI_USE_DREG_DEFAULT to 1 when using the ibv backe
  nd
- improve(cmake): improve the LCI_SERVER auto selection: if we find ofi/cxi p
  rovider, use ofi by default
- improve(pmi): add warning to users when PMIx environment is detected but PM
  Ix support is not enabled.
- improve(ofi): use try lock wrapping the endpoint when posting sends and puts.

## Fixed Issues
- Fix LCI compilation on AArch64.
- fix(ibv): fix compilation warning of "non-void function does not return a v
  alue".
- fix(lci-ucx): fix compilation warning "macro expansion producing 'defined'
  has undefined behavior".
- fix(lci-ucx): add missing compilation options (especially -DUSE_LOCKS=1) wh
  en using ptmalloc.
- fix(rcache): fix compilation warning of non-void function no return.
- fix(ofi): correctly handle the case when fi_recv returns -FI_EAGAIN.
- fix(pmi): use cray/slurm pmi if found; update the embedded pmi API to new version.