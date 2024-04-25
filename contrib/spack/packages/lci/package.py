from spack import *
import os

class Lci(CMakePackage):
    """LCI: the Lightweight Communication Interface"""

    homepage = 'https://github.com/uiuc-hpc/lci'
    url      = 'https://github.com/uiuc-hpc/lci/archive/refs/tags/v1.7.7.tar.gz'
    git      = 'https://github.com/uiuc-hpc/lci.git'

    maintainers('omor1', 'JiakunYan')

    version('master', branch='master')
    version('coll', branch='topic/coll')
    version("1.7.7", sha256="3bdff45eb0ccff5d89863f6c7eb0000e702f4b63b37c8db601d7be257417dc5a")
    version('1.7.6', sha256='2cc6ebd2034cbdc4a1e6da3f0121092e27690851749bfa59ee70fa82309a8a2a')
    version('1.7.5', sha256='a7a2aaec8ed2de31dabfbcbf1ff4dda219cb6c49a661a9072a8364ca10fe622c')
    version('1.7.4', sha256='843f53c4d5f00653b466ca142679d2652cc93205e021d8d890513b3a44c44669')
    version('1.7.3', sha256='329c24e75d9e61a694b7ff3cebe57272b6e39204fa10c7d4db151183f04b5fe8')

    def is_positive_int(val):
        try:
            return int(val) > 0
        except ValueError:
            return val == 'auto'

    variant('fabric', default='ibv', values=('ofi', 'ibv', 'ucx'), multi=False,
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
    variant('default-dreg', default='auto', values=is_positive_int,
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
    variant('enable-pm', description='Which process management backend to enable',
            values=disjoint_sets(
                ('auto',), ('pmix', 'pmi2', 'pmi1', 'mpi', 'local'),
            ).prohibit_empty_set(
            ).with_default('auto').with_non_feature_values('auto'))

    generator('ninja', 'make', default='ninja')

    depends_on('cmake@3.19:', type='build')
    depends_on('libfabric', when='fabric=ofi')
    depends_on('rdma-core', when='fabric=ibv')
    depends_on('ucx', when='fabric=ucx')
    depends_on('mpi', when='default-pm=mpi')
    depends_on('mpi', when='enable-pm=mpi')
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
            self.define_from_variant('LCI_DEBUG', 'debug'),
            self.define_from_variant('LCI_USE_PERFORMANCE_COUNTER', 'pcounter'),
            self.define_from_variant('LCI_ENABLE_SLOWDOWN', 'debug-slow'),
            self.define_from_variant('LCI_USE_PAPI', 'papi'),
            self.define_from_variant('LCI_USE_GPROF', 'gprof'),
        ]

        if not self.spec.satisfies('default-dreg=auto'):
            arg = self.define_from_variant('LCI_USE_DREG_DEFAULT', 'default-dreg')
            args.append(arg)

        if not self.spec.satisfies('enable-pm=auto'):
            arg = self.define('LCT_PMI_BACKEND_ENABLE_PMI1', 'enable-pm=pmi1' in self.spec)
            args.append(arg)
            arg = self.define('LCT_PMI_BACKEND_ENABLE_PMI2', 'enable-pm=pmi2' in self.spec)
            args.append(arg)
            arg = self.define('LCT_PMI_BACKEND_ENABLE_MPI', 'enable-pm=mpi' in self.spec)
            args.append(arg)
            arg = self.define('LCT_PMI_BACKEND_ENABLE_PMIX', 'enable-pm=pmix' in self.spec)
            args.append(arg)

        if not self.spec.satisfies('cache-line=auto'):
            arg = self.define_from_variant('LCI_CACHE_LINE', 'cache-line')
            args.append(arg)

        if self.spec.satisfies('platform=cray') or self.spec.satisfies('default-pm=cray'):
            arg = self.define('LCI_PMI_BACKEND_DEFAULT', 'pmi1')
            args.append(arg)
        elif not self.spec.satisfies('default-pm=auto'):
            arg = self.define_from_variant('LCI_PMI_BACKEND_DEFAULT', 'default-pm')
            args.append(arg)

        if not self.spec.satisfies('default-packet-size=auto'):
            arg = self.define_from_variant('LCI_PACKET_SIZE_DEFAULT', 'default-packet-size')
            args.append(arg)

        if not self.spec.satisfies('default-packets=auto'):
            arg = self.define_from_variant('LCI_SERVER_NUM_PKTS_DEFAULT', 'default-packets')
            args.append(arg)

        if not self.spec.satisfies('default-max-sends=auto'):
            arg = self.define_from_variant('LCI_SERVER_MAX_SENDS_DEFAULT', 'default-max-sends')
            args.append(arg)

        if not self.spec.satisfies('default-max-recvs=auto'):
            arg = self.define_from_variant('LCI_SERVER_MAX_RECVS_DEFAULT', 'default-max-recvs')
            args.append(arg)

        if not self.spec.satisfies('default-max-cqe=auto'):
            arg = self.define_from_variant('LCI_SERVER_MAX_CQES_DEFAULT', 'default-max-cqe')
            args.append(arg)

        return args
