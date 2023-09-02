from spack import *
import os

class Lci(CMakePackage):
    """LCI: the Lightweight Communication Interface"""

    homepage = "https://github.com/uiuc-hpc/LC"
    url      = "https://github.com/uiuc-hpc/LC/archive/refs/tags/v1.7.4.tar.gz"
    git      = "git@github.com:uiuc-hpc/LC.git"

    version('master', branch='master')
    version('coll', branch='topic/coll')

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
                ('auto',), ('pmix', 'pmi2', 'pmi1', 'mpi', 'local')
            ).prohibit_empty_set(
            ).with_default('auto').with_non_feature_values('auto'))
    variant('multithread-progress', default=False,
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
    variant('debug-perfcounter', default=False,
            description='Use performance counter')
    variant('debug-slow', default=False, description='Enable manual slowdown')
    variant('papi', default=False,
            description='Use PAPI to collect hardware counters')
    variant('gprof', default=False, description='Enable GPROF')

    generator = 'Ninja'
    depends_on('ninja', type='build')

    depends_on('cmake@3.19:', type='build')
    depends_on('libfabric', when='fabric=ofi')
    depends_on('rdma-core', when='fabric=ibv')
    depends_on('mpi', when='default-pm=mpi')
    depends_on('papi', when='+papi')
    depends_on('doxygen', when='+docs')

    @property
    def build_directory(self):
        return os.path.join(self.stage.path, 'spack-build', self.spec.dag_hash(7))

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
            self.define_from_variant('LCI_USE_PERFORMANCE_COUNTER', 'debug-perfcounter'),
            self.define_from_variant('LCI_ENABLE_SLOWDOWN', 'debug-slow'),
            self.define_from_variant('LCI_USE_PAPI', 'papi'),
            self.define_from_variant('LCI_USE_GPROF', 'gprof'),
        ]

        if self.spec.variants['cache-line'].value != 'auto':
            arg = self.define_from_variant('LCI_CACHE_LINE', 'cache-line')
            args.append(arg)

        if 'auto' not in self.spec.variants['default-pm'].value:
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
