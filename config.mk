# This file contains all customized compile options for LCI
# WIP, there are more configs in ./src/include/confg.h

# Define server here (ofi, ibv, psm)
LCI_SERVER ?= psm

# When in doubt, enable this (yes, no)
LCI_SERVER_DEBUG ?= no

# Use registration cache ? (Only affect ibv -- only use when not-so-dynamic allocation)
LCI_USE_DREG ?= yes

# Addressing mode "dynamic, explicit, immediate"
LCI_EP_AR = "explicit"

# Completion mechanism "sync, cq, am, glob"
LCI_EP_CE = "sync"

# Maximum number of devices.
LCI_MAX_DEV = 4

# Maximum number of EP.
LCI_MAX_EP = 8
