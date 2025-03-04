#ifndef LCI_LCI_INTERNAL_HPP
#define LCI_LCI_INTERNAL_HPP

#include <memory>
#include <vector>
#include <list>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <algorithm>

#include "lci.hpp"
#include "lct.h"
#include "util/log.hpp"
#include "util/random.hpp"
#include "util/misc.hpp"
#include "util/spinlock.hpp"
#include "monitor/performance_counter.hpp"
#include "network/network.hpp"
#ifdef LCI_BACKEND_ENABLE_IBV
#include "network/ibv/backend_ibv.hpp"
#endif
#ifdef LCI_BACKEND_ENABLE_OFI
#include "network/ofi/backend_ofi.hpp"
#endif
#include "data_structure/mpmc_array.hpp"
#include "data_structure/mpmc_set.hpp"
#include "matching_engine/matching_engine.hpp"
#include "core/data.hpp"
#include "core/rhandler_registry.hpp"
#include "core/global.hpp"
#include "core/protocol.hpp"
#include "core/cq.hpp"
#include "core/packet_pre.hpp"
#include "core/packet_pool.hpp"
#include "core/runtime.hpp"
#include "core/rendezvous.hpp"

// inline implementation
#include "network/network_inline.hpp"
#include "network/device_inline.hpp"
#include "network/endpoint_inline.hpp"
#include "core/packet_inline.hpp"
#ifdef LCI_BACKEND_ENABLE_IBV
#include "network/ibv/backend_ibv_inline.hpp"
#endif
#ifdef LCI_BACKEND_ENABLE_OFI
#include "network/ofi/backend_ofi_inline.hpp"
#endif

#endif  // LCI_LCI_INTERNAL_HPP