.PHONY: all clean install

include config.mk

MPICC ?= mpicc
CC ?= gcc
CFLAGS ?= -g3 -ggdb -O3 -Wall -Wextra
AR ?= ar
RANLIB ?= ranlib
PREFIX ?= /usr

SRCDIR = ./src
OBJDIR ?= ./obj

INCLUDE = -I./include -I./src/include -I./  -I./src/include/umalloc -I./src/
CFLAGS += -fPIC -fvisibility=hidden -std=gnu99 $(INCLUDE) $(SERVER) -DUSE_AFFI -D_GNU_SOURCE -pthread

IBV_INC = -I/opt/ofed/include/ -I$(HOME)/libfab/include
IBV_LIB = -L/opt/ofed/lib64 -libverbs

JFCONTEXT = include/ult/fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = include/ult/fult/make_x86_64_sysv_elf_gas.S

CFLAGS += $(IBV_INC)
LDFLAGS += -shared -Lstatic -flto $(IBV_LIB) # -llmpe -lmpe

### FOR USING PAPI ##
# PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
# PAPI_INC = $(TACC_PAPI_INC)
# CFLAGS += -I$(PAPI_INC)
# LDFLAGS += -L$(PAPI_LIB)

FCONTEXT = mfcontext.o jfcontext.o
COMM = lc.o mpiv.o progress.o hashtable.o pool.o lcrq.o
DREG = dreg/dreg.o dreg/avl.o

OBJECTS = $(addprefix $(OBJDIR)/, $(COMM))

LIBOBJ = $(OBJECTS) $(addprefix $(OBJDIR)/, $(FCONTEXT))

ifeq ($(LC_SERVER), ofi)
	CFLAGS += -DLC_USE_SERVER_OFI -DAFF_DEBUG
	LDFLAGS += -lfabric # -L$(HOME)/libfab/lib -lfabric
	LIBOBJ += $(addprefix $(OBJDIR)/, $(DREG))
endif

ifeq ($(LC_SERVER), ibv)
	CFLAGS += -DLC_USE_SERVER_IBV -DAFF_DEBUG
	LIBOBJ += $(addprefix $(OBJDIR)/, $(DREG))
endif

ifeq ($(LC_SERVER), psm)
	CFLAGS += -DLC_USE_SERVER_PSM -DAFF_DEBUG
	LDFLAGS += -L/home1/03490/tg827895/psm2/usr/lib64/ -lpsm2 # -L$(HOME)/libfab/lib -lfabric
	LIBOBJ += $(addprefix $(OBJDIR)/, $(DREG))
endif

LIBRARY = liblwci.so
ARCHIVE = liblwci.a

all: $(LIBRARY) $(ARCHIVE)

install: all
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include
	cp -R include/* $(PREFIX)/include
	cp libmv.a $(PREFIX)/lib
	cp libmv.so $(PREFIX)/lib

$(OBJDIR)/%.o: $(SRCDIR)/$(notdir %.c)
	$(MPICC) $(CFLAGS) -c $< -o $@

$(LIBRARY): $(LIBOBJ)
	$(MPICC) $(LDFLAGS) $(LIBOBJ) $(MALLOC) -o $(LIBRARY)


$(ARCHIVE): $(LIBOBJ)
	@rm -f $(ARCHIVE)
	$(AR) q $(ARCHIVE) $(LIBOBJ) $(MALLOC)
	$(RANLIB) $(ARCHIVE)

$(OBJDIR)/jfcontext.o: $(JFCONTEXT)
	$(CC) -O3 -c $(JFCONTEXT) -o $(OBJDIR)/jfcontext.o

$(OBJDIR)/mfcontext.o: $(MFCONTEXT)
	$(CC) -O3 -c $(MFCONTEXT) -o $(OBJDIR)/mfcontext.o

clean:
	rm -rf $(LIBOBJ) libmv.a libmv.so

