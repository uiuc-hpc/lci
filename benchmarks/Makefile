MPICXX = mpicxx
GXX = gcc
CXXFLAGS += -O3 -Wall -Wextra -std=c++11 -I../ult -I../ -DUSE_AFFI

### FOR TESTING HASH TABLE ###
#FACEBOOK_INC = -I$(HOME)/facebook/include -I$(HOME)/glog/include -I$(HOME)/folly/folly/double-conversion #-DNDEBUG
#FACEBOOK_LIB = -L$(HOME)/gflags/lib -lgflags -L$(HOME)/glog/lib -lglog -L$(HOME)/facebook/lib -lfolly -lpthread
#CXXFLAGS += $(FACEBOOK_INC)
#LDFLAGS += $(FACEBOOK_LIB)

### FOR TESTING LIBCUCKOO ###
# CUCKOO_INC = $(HOME)/libcuckoo/include
# CXXFLAGS += -I$(CUCKOO_INC)

### FOR TESTING TBB ###
#TBB_INC = -I/opt/apps/intel/15/composer_xe_2015.2.164/tbb/include/
#TBB_LIB = -ltbb -ltbbmalloc
#LDFLAGS += $(TBB_LIB)
#CXXFLAGS += $(TBB_INC)

BOOST_INC = $(TACC_BOOST_INC) #/usr/local/boost/1.58.0/include
BOOST_LIB = $(TACC_BOOST_LIB)
CXXFLAGS += -Wno-unused-local-typedefs

### FOR TESTING ARGOBOTS ###
ABT_INC = $(HOME)/argo/include
ABT_LIB = $(HOME)/argo/lib -labt
CXXFLAGS += -I$(ABT_INC)
LDFLAGS += -L$(ABT_LIB)

### FOR USING PAPI ##
# PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
# PAPI_INC = $(TACC_PAPI_INC)
# CXXFLAGS += -I$(PAPI_INC)
# LDFLAGS += -L$(PAPI_LIB)

RDMAX_INC = ../rdmax/include
IBV_INC = /opt/ofed/include/
IBV_LIB = /opt/ofed/lib64 -libverbs

JFCONTEXT = ../ult/fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = ../ult/fult/make_x86_64_sysv_elf_gas.S

CXXFLAGS_NOBOOST = -I$(ABT_INC)

CXXFLAGS += -I$(BOOST_INC) -I$(RDMAX_INC) -I$(IBV_INC)
LDFLAGS += -L$(BOOST_LIB) -L$(IBV_LIB) # -llmpe -lmpe

all: pp 

pp: pingpong pingpong_cond_nonuma pingpong_nonuma pingpong_abt_nonuma \
		pingpong_mt pingpong_mt_pthread pingpong_mt_cond_nonuma pingpong_mt_nonuma pingpong_mt_abt_nonuma

pingpong_abt_nonuma: CXXFLAGS+=-DUSE_ABT -DCONFIG_PK_MANAGER=packet_manager_LFSTACK
pingpong_mt_abt_nonuma: CXXFLAGS+=-DUSE_ABT -DCONFIG_PK_MANAGER=packet_manager_LFSTACK

pingpong_cond_nonuma: CXXFLAGS+=-DUSE_WORKER_WAIT -DCONFIG_PK_MANAGER=packet_manager_LFSTACK
pingpong_mt_cond_nonuma: CXXFLAGS+=-DUSE_WORKER_WAIT -DCONFIG_PK_MANAGER=packet_manager_LFSTACK

pingpong_nonuma: CXXFLAGS+=-DCONFIG_PK_MANAGER=packet_manager_LFSTACK
pingpong_mt_nonuma: CXXFLAGS+=-DCONFIG_PK_MANAGER=packet_manager_LFSTACK

abt_%: abt_%.c
	$(GXX) $(CXXFLAGS) $^ -o $@ -L$(ABT_LIB)

abx_%: abx_%.cxx
	$(MPICXX) $(CXXFLAGS_NOBOOST) $^ -o $@ -L$(ABT_LIB)

%: mfcontext.o jfcontext.o %.cxx ../*.h ../ult/*.h ../ult/fult/*.h ../ult/abt/*.h
	$(MPICXX) $(CXXFLAGS) mfcontext.o jfcontext.o $@.cxx $(LDFLAGS) -o $@

jfcontext.o: $(JFCONTEXT)
	$(GXX) -O2 -c $(JFCONTEXT) -o jfcontext.o

mfcontext.o: $(MFCONTEXT)
	$(GXX) -O2 -c $(MFCONTEXT) -o mfcontext.o

jacobi_3d: jacobi_3d.c
	$(MPICXX) -O3 $^ -o $@

clean:
	rm -rf $(TARGET) *.o
