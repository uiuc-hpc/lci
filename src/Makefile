MPICXX = mpicxx
CXX = g++

INCLUDE = -I../include -I../ -I../ult/ -I../include/mv/

CFLAGS ?= -Wall -std=gnu99 -O3 -g $(INCLUDE)
CXXFLAGS ?= -g -ggdb -O3 -Wall -Wextra -std=c++11 $(INCLUDE) -DUSE_AFFI

IBV_INC = -I/opt/ofed/include/ -I$(HOME)/libfab/include

JFCONTEXT = ../ult/fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = ../ult/fult/make_x86_64_sysv_elf_gas.S

CXXFLAGS += $(IBV_INC)
LDFLAGS += $(IBV_LIB) # -llmpe -lmpe

FCONTEXT = mfcontext.o jfcontext.o
COMM = init.o
MPIV = mpiv.o

all: libmv.a

libmv.a: $(FCONTEXT) $(COMM) $(MPIV)
	ar rcs $@ $^

%.o: %.cxx
	$(MPICXX) $(CXXFLAGS) -c $^ -o $@

jfcontext.o: $(JFCONTEXT)
	$(CXX) -O3 -c $(JFCONTEXT) -o jfcontext.o

mfcontext.o: $(MFCONTEXT)
	$(CXX) -O3 -c $(MFCONTEXT) -o mfcontext.o

clean:
	rm -rf *.o libmv.a
