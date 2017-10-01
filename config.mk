# This file contains all customized compile options for LWCI
# WIP, there are more configs in ./src/include/confg.h

# Define server here (ofi, ibv, psm)
LC_SERVER ?= ibv

# Define use inline send or not (yes, no)
# Could help latency of tiny messages, but may fail due to lack of flow control
LC_SERVER_INLINE ?= yes

# When in doubt, enable this (yes, no)
LC_SERVER_DEBUG ?= no

# Use registration cache ? (Only affect ibv -- only use when not-so-dynamic allocation)
LC_USE_DREG ?= yes
