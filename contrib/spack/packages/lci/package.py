from spack import *
import os

class Lci(CMakePackage):
    """LCI: the Lightweight Communication Interface"""

    homepage = "https://github.com/uiuc-hpc/LC"
    url      = "https://github.com/uiuc-hpc/LC/archive/refs/tags/v1.7.6.tar.gz"
    git      = "https://github.com/uiuc-hpc/LC.git"

    maintainers("omor1", "JiakunYan")

    version('master', branch='master')
    version('coll', branch='topic/coll')
    version("1.7.6", sha256="2cc6ebd2034cbdc4a1e6da3f0121092e27690851749bfa59ee70fa82309a8a2a")
    version("1.7.5", sha256="a7a2aaec8ed2de31dabfbcbf1ff4dda219cb6c49a661a9072a8364ca10fe622c")
    version("1.7.4", sha256="843f53c4d5f00653b466ca142679d2652cc93205e021d8d890513b3a44c44669")
    version("1.7.3", sha256="329c24e75d9e61a694b7ff3cebe57272b6e39204fa10c7d4db151183f04b5fe8")

    def is_positive_int(val):
        try:
            return int(val) > 0
        except ValueError:
            return val == 'auto'

    variant('fabric', default='ibv', values=('ofi', 'ibv'), multi=False,
            description='Communication fabric')
    variant('completion', default='sync,cq,am',
            values=('sync', 'cq', 'am', 'glob'), multi=True,
            description='Completion mechanism')

    variant('shared', default=True,  description='Build with shared libraries')
    variant('examples', default=False, description='Build LCI examples')
    variant('tests', default=False, description='Build LCI tests')
    variant('benchmarks', default=False, description='Build LCI benchmarks')
    variant('docs', default=False, description='Build LCI documentation')

    variant('vector', default=True,
            description='Use GCC vector extension for the immediate field')
    variant('aligned', default=True, description='Enable memory alignment')
    variant('cache-line', default='auto', values=is_positive_int,
            description='Cache line size, in bytes')
    variant('native', default=True, description='Build with -march=native')

    variant('inline-cq', default=False,
            description='Use the inline C completion queue implementation')
    variant('ibv-td', default=True,
            description='Enable IBV thread domain')

    variant('default-pm', description='Default: Process management backend',
            values=disjoint_sets(
                ('auto',), ('pmix', 'pmi2', 'pmi1', 'mpi', 'local'), ('cray',),
            ).prohibit_empty_set(
            ).with_default('auto').with_non_feature_values('auto'))
    variant('multithread-progress', default=True,
            description='Enable thread-safe LCI_progress function')
    variant('default-dreg', default=True,
            description='Default: Whether to use registration cache')
    variant('default-packet-size', default='auto', values=is_positive_int,
            description='Default: Size of packet')
    variant('default-packets', default='auto', values=is_positive_int,
            description='Default: Number of packets')
    variant('default-max-sends', default='auto', values=is_positive_int,
            description='Default: Max posted sends')
    variant('default-max-recvs', default='auto', values=is_positive_int,
            description='Default: Max posted recvs')
    variant('default-max-cqe', default='auto', values=is_positive_int,
            description='Default: Max posted cqes')

    variant('debug', default=False, description='Enable debug mode')
    variant('pcounter', default=False,
            description='Use performance counter')
    variant('debug-slow', default=False, description='Enable manual slowdown')
    variant('papi', default=False,
            description='Use PAPI to collect hardware counters')
    variant('gprof', default=False, description='Enable GPROF')

    generator("ninja", "make", default="ninja")

    depends_on('cmake@3.19:', type='build')
    depends_on('libfabric', when='fabric=ofi')
    depends_on('rdma-core', when='fabric=ibv')
    depends_on('mpi', when='default-pm=mpi')
    depends_on('papi', when='+papi')
    depends_on('doxygen', when='+docs')
    depends_on('cray-mpich', when='platform=cray')
    depends_on('cray-pmi', when='default-pm=cray')

    def cmake_args(self):
        args = [
            self.define_from_variant('LCI_SERVER', 'fabric'),
            self.define('LCI_FORCE_SERVER', True),
            self.define_from_variant('LCI_EP_CE', 'completion'),
            self.define_from_variant('BUILD_SHARED_LIBS', 'shared'),
            self.define_from_variant('LCI_WITH_EXAMPLES', 'examples'),
            self.define_from_variant('LCI_WITH_TESTS', 'tests'),
            self.define_from_variant('LCI_WITH_BENCHMARKS', 'benchmarks'),
            self.define_from_variant('LCI_WITH_DOC', 'docs'),
            self.define_from_variant('LCI_USE_AVX', 'vector'),
            self.define_from_variant('LCI_CONFIG_USE_ALIGNED_ALLOC', 'aligned'),
            self.define_from_variant('LCI_OPTIMIZE_FOR_NATIVE', 'native'),
            self.define_from_variant('LCI_USE_INLINE_CQ', 'inline-cq'),
            self.define_from_variant('LCI_IBV_ENABLE_TD', 'ibv-td'),
            self.define_from_variant('LCI_ENABLE_MULTITHREAD_PROGRESS', 'multithread-progress'),
            self.define('LCI_USE_DREG_DEFAULT',
                        1 if self.spec.variants['default-dreg'].value else 0),
            self.define_from_variant('LCI_DEBUG', 'debug'),
            self.define_from_variant('LCI_USE_PERFORMANCE_COUNTER', 'pcounter'),
            self.define_from_variant('LCI_ENABLE_SLOWDOWN', 'debug-slow'),
            self.define_from_variant('LCI_USE_PAPI', 'papi'),
            self.define_from_variant('LCI_USE_GPROF', 'gprof'),
        ]

        if self.spec.variants['cache-line'].value != 'auto':
            arg = self.define_from_variant('LCI_CACHE_LINE', 'cache-line')
            args.append(arg)

        if self.spec.satisfies("platform=cray") or self.spec.satisfies("default-pm=cray"):
            arg = self.define('LCI_PMI_BACKEND_DEFAULT', 'pmi1')
            args.append(arg)
        elif 'auto' not in self.spec.variants['default-pm'].value:
            arg = self.define_from_variant('LCI_PMI_BACKEND_DEFAULT', 'default-pm')
            args.append(arg)

        if self.spec.variants['default-packet-size'].value != 'auto':
            arg = self.define_from_variant('LCI_PACKET_SIZE_DEFAULT', 'default-packet-size')
            args.append(arg)

        if self.spec.variants['default-packets'].value != 'auto':
            arg = self.define_from_variant('LCI_SERVER_NUM_PKTS_DEFAULT', 'default-packets')
            args.append(arg)

        if self.spec.variants['default-max-sends'].value != 'auto':
            arg = self.define_from_variant('LCI_SERVER_MAX_SENDS_DEFAULT', 'default-max-sends')
            args.append(arg)

        if self.spec.variants['default-max-recvs'].value != 'auto':
            arg = self.define_from_variant('LCI_SERVER_MAX_RECVS_DEFAULT', 'default-max-recvs')
            args.append(arg)

        if self.spec.variants['default-max-cqe'].value != 'auto':
            arg = self.define_from_variant('LCI_SERVER_MAX_CQES_DEFAULT', 'default-max-cqe')
            args.append(arg)

        return args
