TARGET = yield wait abt_yield abt_wait fibo abt_fibo fair \
    comm abx_comm abx_comm2 abx_comm_order abx_comm_reorder \
    abt_fair comm3 comm4 abx_comm4 abx_comm42

MPICXX = mpicxx
GXX = gcc
CXXFLAGS += -g3 -O3 -Wall -Wextra -I../fult -I../\
		#-DNDEBUG

LDFLAGS = -L/opt/ofed/lib64 -libverbs

CUCKOO_INC = $(HOME)/libcuckoo/include

BOOST_INC = $(TACC_BOOST_INC) #/usr/local/boost/1.58.0/include
BOOST_LIB = $(TACC_BOOST_LIB) 

ABT_INC = $(HOME)/argo/include
ABT_LIB = $(HOME)/argo/lib -labt

RDMAX_INC = ../rdmax/include

IBV_INC = /opt/ofed/include/

JFCONTEXT = ../fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = ../fult/make_x86_64_sysv_elf_gas.S

CXXFLAGS_NOBOOST = -I$(ABT_INC)

CXXFLAGS += -I$(BOOST_INC) -I$(ABT_INC) -I$(CUCKOO_INC) -I$(RDMAX_INC) -I$(IBV_INC) -I$(TACC_PAPI_INC) -fsplit-stack -DBOOST_USE_SEGMENTED_STACKS
LDFLAGS += -L$(TACC_PAPI_LIB) -lpapi -L$(BOOST_LIB)

all: $(TARGET)

abt_%: abt_%.c
	$(GXX) $(CXXFLAGS) $^ -o $@ -L$(ABT_LIB)

abx_%: abx_%.cxx
	$(MPICXX) $(CXXFLAGS_NOBOOST) $^ -o $@ -L$(ABT_LIB)

%: mfcontext.o jfcontext.o %.cxx
	$(MPICXX) $(CXXFLAGS) $(LDFLAGS) -std=c++11 $^ -o $@

segmented_stack.o: ../segmented_stack.cxx
	$(CXX) $(CXXFLAGS) -c ../segmented_stack.cxx

jfcontext.o: $(JFCONTEXT)
	$(GXX) -O2 -c $(JFCONTEXT) -o jfcontext.o

mfcontext.o: $(MFCONTEXT)
	$(GXX) -O2 -c $(MFCONTEXT) -o mfcontext.o

clean:
	rm -rf $(TARGET) *.o
