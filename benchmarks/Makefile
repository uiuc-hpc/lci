TARGET = yield wait abt_yield abt_wait fibo abt_fibo fair \
    comm abx_comm abx_comm2 abx_comm_order abx_comm_reorder \
    abt_fair comm3 comm4 abx_comm4 abx_comm42

MPICXX = mpicxx
GXX = gcc
CXXFLAGS += -g3 -O3 -Wall -Wextra -I../fult -I../\
		#-DNDEBUG

LDFLAGS += -L/opt/ofed/lib64 -libverbs #-L/opt/apps/intel/15/composer_xe_2015.2.164/tbb/lib/intel64/gcc4.4/ -ltbb -ltbbmalloc

CUCKOO_INC = $(HOME)/libcuckoo/include
TBB_INC = /opt/apps/intel/15/composer_xe_2015.2.164/tbb/include/

BOOST_INC = $(TACC_BOOST_INC) #/usr/local/boost/1.58.0/include
BOOST_LIB = $(TACC_BOOST_LIB)

ABT_INC = $(HOME)/argo/include
ABT_LIB = $(HOME)/argo/lib -labt

RDMAX_INC = ../rdmax/include

IBV_INC = /opt/ofed/include/

JFCONTEXT = ../fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = ../fult/make_x86_64_sysv_elf_gas.S

CXXFLAGS_NOBOOST = -I$(ABT_INC)

CXXFLAGS += -std=c++11  -I$(BOOST_INC) -I$(ABT_INC) -I$(CUCKOO_INC) -I$(TBB_INC) -I$(RDMAX_INC) -I$(IBV_INC) -I$(TACC_PAPI_INC) #-fsplit-stack -DBOOST_USE_SEGMENTED_STACKS
LDFLAGS += -L$(TACC_PAPI_LIB) -lpapi -L$(BOOST_LIB)# -llmpe -lmpe

all: $(TARGET)

abt_%: abt_%.c
	$(GXX) $(CXXFLAGS) $^ -o $@ -L$(ABT_LIB)

abx_%: abx_%.cxx
	$(MPICXX) $(CXXFLAGS_NOBOOST) $^ -o $@ -L$(ABT_LIB)

%: mfcontext.o jfcontext.o %.cxx
	$(MPICXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

jfcontext.o: $(JFCONTEXT)
	$(GXX) -O2 -c $(JFCONTEXT) -o jfcontext.o

mfcontext.o: $(MFCONTEXT)
	$(GXX) -O2 -c $(MFCONTEXT) -o mfcontext.o

jacobi_3d: jacobi_3d.c
	$(MPICXX) -O3 $^ -o $@

clean:
	rm -rf $(TARGET) *.o
