.PHONY: all clean install

include config.mk

CC ?= gcc
CFLAGS ?= -g3 -ggdb -O3 -Wall -Wextra
AR ?= ar
RANLIB ?= ranlib
PREFIX ?= /usr

SRCDIR = ./src
OBJDIR ?= ./obj

EXTRA = -DAFF_DEBUG -DUSE_AFFI

ifeq (,$(wildcard $(OBJDIR)))
$(shell mkdir -p $(OBJDIR))
endif

ifeq (,$(wildcard $(OBJDIR)/pmi))
$(shell mkdir -p $(OBJDIR)/pmi)
endif

ifeq (,$(wildcard $(OBJDIR)/dreg))
$(shell mkdir -p $(OBJDIR)/dreg)
endif

INCLUDE = -I./include -I./src/include -I./  -I./src/ 
CFLAGS += -fPIC -fvisibility=hidden -std=gnu99 $(INCLUDE) $(EXTRA) -D_GNU_SOURCE -pthread

IBV_DIR = /opt/ofed/
PSM_DIR = /usr/

CFLAGS += $(IBV_INC)
LDFLAGS += -shared -Lstatic

### FOR USING PAPI ##
# PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
# PAPI_INC = $(TACC_PAPI_INC)
# CFLAGS += -I$(PAPI_INC)
# LDFLAGS += -L$(PAPI_LIB)

ifeq ($(LC_SERVER_DEBUG), yes)
CFLAGS += -DLC_SERVER_DEBUG
endif

ifeq ($(LC_SERVER_INLINE), yes)
CFLAGS += -DLC_SERVER_INLINE
endif

ifeq ($(LC_SERVER), ofi)
	CFLAGS += -DLC_USE_SERVER_OFI -DAFF_DEBUG
endif

ifeq ($(LC_SERVER), ibv)
	CFLAGS += -DLC_USE_SERVER_IBV -DAFF_DEBUG -I$(IBV_DIR)/include
endif

ifeq ($(LC_SERVER), psm)
	CFLAGS += -DLC_USE_SERVER_PSM -DAFF_DEBUG -I$(PSM_DIR)/include
endif


JFCONTEXT = include/ult/fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = include/ult/fult/make_x86_64_sysv_elf_gas.S
FCONTEXT = mfcontext.o jfcontext.o

COMM = lc.o mpiv.o progress.o hashtable.o pool.o lcrq.o
DREG = dreg/dreg.o dreg/avl.o ptmalloc283/malloc.o
PMI = pmi/simple_pmi.o pmi/simple_pmiutil.o

# USE DREG
LIBOBJ += $(addprefix $(OBJDIR)/, $(DREG))

OBJECTS = $(addprefix $(OBJDIR)/, $(COMM))

LIBOBJ += $(OBJECTS) $(addprefix $(OBJDIR)/, $(FCONTEXT)) $(addprefix $(OBJDIR)/, $(PMI))

LIBRARY = liblwci.so
ARCHIVE = liblwci.a

all: $(LIBRARY) $(ARCHIVE)

install: all
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include
	cp -R include/* $(PREFIX)/include
	cp liblwci.a $(PREFIX)/lib
	cp liblwci.so $(PREFIX)/lib

$(OBJDIR)/%.o: $(SRCDIR)/$(notdir %.c)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBRARY): $(LIBOBJ)
	$(CC) $(LDFLAGS) $(LIBOBJ) $(MALLOC) -o $(LIBRARY)


$(ARCHIVE): $(LIBOBJ)
	@rm -f $(ARCHIVE)
	$(AR) q $(ARCHIVE) $(LIBOBJ) $(MALLOC)
	$(RANLIB) $(ARCHIVE)

$(OBJDIR)/jfcontext.o: $(JFCONTEXT)
	$(CC) -O3 -c $(JFCONTEXT) -o $(OBJDIR)/jfcontext.o

$(OBJDIR)/mfcontext.o: $(MFCONTEXT)
	$(CC) -O3 -c $(MFCONTEXT) -o $(OBJDIR)/mfcontext.o

clean:
	rm -rf $(LIBOBJ) liblwci.a liblwci.so

