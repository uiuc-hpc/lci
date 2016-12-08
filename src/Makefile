MPICC = mpicc
CXX = gcc

INCLUDE = -I../include -I../ -I./include

CFLAGS ?= -g3 -ggdb -O3 -Wall -Wextra -std=gnu99 $(INCLUDE) -DUSE_AFFI -D_GNU_SOURCE -pthread

IBV_INC = -I/opt/ofed/include/ -I$(HOME)/libfab/include

JFCONTEXT = ../ult/fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = ../ult/fult/make_x86_64_sysv_elf_gas.S

CFLAGS += $(IBV_INC)
LDFLAGS += $(IBV_LIB) # -llmpe -lmpe

### FOR USING PAPI ##
# PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
# PAPI_INC = $(TACC_PAPI_INC)
# CFLAGS += -I$(PAPI_INC)
# LDFLAGS += -L$(PAPI_LIB)

FCONTEXT = mfcontext.o jfcontext.o
COMM = mv.o mpiv.o progress.o hashtable.o pool.o # server_ibv.o

all: libmv.a

libmv.a: $(FCONTEXT) $(COMM)
	ar rcs $@ $^

%.o: %.c
	$(MPICC) $(CFLAGS) -c $^ -o $@

jfcontext.o: $(JFCONTEXT)
	$(CXX) -O3 -c $(JFCONTEXT) -o jfcontext.o

mfcontext.o: $(MFCONTEXT)
	$(CXX) -O3 -c $(MFCONTEXT) -o mfcontext.o

clean:
	rm -rf *.o libmv.a
