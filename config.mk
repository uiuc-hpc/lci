# This file contains all customized compile options for LCI
# WIP, there are more configs in ./src/include/confg.h

# Define server here (ofi, ibv, psm)
LC_SERVER ?= psm

# When in doubt, enable this (yes, no)
LC_SERVER_DEBUG ?= no

# Use registration cache ? (Only affect ibv -- only use when not-so-dynamic allocation)
LC_USE_DREG ?= yes

# Addressing mode "dynamic, explicit, immediate"
LC_EP_AR = "dynamic, explicit"

# Completion mechanism "sync, cq, am, glob"
LC_EP_CE = "sync, cq"
