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

INCLUDE = -I./include -I./src/include -I./  -I./src/include/umalloc
CFLAGS += -fPIC -fvisibility=hidden -std=gnu99 $(INCLUDE) $(SERVER) -DUSE_AFFI -D_GNU_SOURCE -pthread

ifeq ($(MV_SERVER), ofi)
	CFLAGS += -DMV_USE_SERVER_OFI
else
	CFLAGS += -DMV_USE_SERVER_IBV
endif

IBV_INC = -I/opt/ofed/include/ -I$(HOME)/libfab/include
IBV_LIB = -L/opt/ofed/lib64 -libverbs # -lfabric

JFCONTEXT = include/mv/ult/fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = include/mv/ult/fult/make_x86_64_sysv_elf_gas.S

CFLAGS += $(IBV_INC)
LDFLAGS += -shared -Lstatic -flto $(IBV_LIB) # -llmpe -lmpe

### FOR USING PAPI ##
# PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
# PAPI_INC = $(TACC_PAPI_INC)
# CFLAGS += -I$(PAPI_INC)
# LDFLAGS += -L$(PAPI_LIB)

FCONTEXT = mfcontext.o jfcontext.o
COMM = mv.o mpiv.o progress.o hashtable.o pool.o
UMALLOC = umalloc/attach.o  umalloc/ufree.o  umalloc/umalloc.o  umalloc/umstats.o

OBJECTS = $(addprefix $(OBJDIR)/, $(COMM))

LIBOBJ = $(OBJECTS) $(addprefix $(OBJDIR)/, $(FCONTEXT)) $(addprefix $(OBJDIR)/, $(UMALLOC))

LIBRARY = libmv.so
ARCHIVE = libmv.a

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
	$(MPICC) $(LDFLAGS) $(LIBOBJ) -o $(LIBRARY)


$(ARCHIVE): $(LIBOBJ)
	@rm -f $(ARCHIVE)
	$(AR) q $(ARCHIVE) $(LIBOBJ)
	$(RANLIB) $(ARCHIVE)

$(OBJDIR)/jfcontext.o: $(JFCONTEXT)
	$(CC) -O3 -c $(JFCONTEXT) -o $(OBJDIR)/jfcontext.o

$(OBJDIR)/mfcontext.o: $(MFCONTEXT)
	$(CC) -O3 -c $(MFCONTEXT) -o $(OBJDIR)/mfcontext.o

clean:
	rm -rf $(OBJDIR)/*.o libmv.a libmv.so

