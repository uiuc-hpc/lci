MPICXX = mpicxx
CXX = g++

INCLUDE = -I../include -I../ -I../ult/ -I../include/mv/

CFLAGS ?= -Wall -std=gnu99 -O3 -g $(INCLUDE)
CXXFLAGS ?= -g -ggdb -O3 -Wall -Wextra -std=c++11 $(INCLUDE) -DUSE_AFFI

# TACC_BOOST_INC=/home/w00d/software/boost_1_61_0/
# TACC_BOOST_LIB=/home/w00d/software/boost_1_61_0/stage/lib

BOOST_INC = -I$(TACC_BOOST_INC) #/usr/local/boost/1.58.0/include
BOOST_LIB = -L$(TACC_BOOST_LIB)
CXXFLAGS += -Wno-unused-local-typedefs

RDMAX_INC = -I../rdmax/include
IBV_INC = -I/opt/ofed/include/ -I$(HOME)/libfab/include
IBV_LIB = -libverbs

JFCONTEXT = ../ult/fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = ../ult/fult/make_x86_64_sysv_elf_gas.S

CXXFLAGS += $(BOOST_INC) $(RDMAX_INC) $(IBV_INC)
LDFLAGS += $(BOOST_LIB) $(IBV_LIB) # -llmpe -lmpe

all: libmv.a

libmv.a: mpiv.o waitall.o init.o memory.o isend.o irecv.o send.o recv.o mfcontext.o jfcontext.o
	ar rcs $@ $^

%.o: %.cxx
	$(MPICXX) $(CXXFLAGS) -c $^ -o $@

jfcontext.o: $(JFCONTEXT)
	$(CXX) -O3 -c $(JFCONTEXT) -o jfcontext.o

mfcontext.o: $(MFCONTEXT)
	$(CXX) -O3 -c $(MFCONTEXT) -o mfcontext.o

clean:
	rm -rf *.o
