MPICXX = mpicc
GXX = gcc
CXXFLAGS += -g -ggdb -O3 -Wall -Wextra -std=gnu99 -I../ult -I../include -I../ -DUSE_AFFI

### FOR TESTING HASH TABLE ###
#FACEBOOK_INC = -I$(HOME)/facebook/include -I$(HOME)/glog/include -I$(HOME)/folly/folly/double-conversion #-DNDEBUG
#FACEBOOK_LIB = -L$(HOME)/gflags/lib -lgflags -L$(HOME)/glog/lib -lglog -L$(HOME)/facebook/lib -lfolly -lpthread
#CXXFLAGS += $(FACEBOOK_INC)
#LDFLAGS += $(FACEBOOK_LIB)

### FOR TESTING LIBCUCKOO ###
CUCKOO_INC = $(HOME)/libcuckoo/include
CXXFLAGS += -I$(CUCKOO_INC)

### FOR TESTING TBB ###
# TBB_INC = -I/opt/apps/intel/15/composer_xe_2015.2.164/tbb/include/
# TBB_LIB = -L/opt/apps/intel/15/composer_xe_2015.2.164/tbb/lib/intel64/gcc4.1 -ltbb -ltbbmalloc
# LDFLAGS += $(TBB_LIB)
# CXXFLAGS += $(TBB_INC)

### FOR TESTING ARGOBOTS ###
# ABT_INC = $(HOME)/argo/include
# ABT_LIB = $(HOME)/argo/lib -labt
# CXXFLAGS += -I$(ABT_INC)
# LDFLAGS += -L$(ABT_LIB)

### FOR USING PAPI ##
PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
PAPI_INC = $(TACC_PAPI_INC)
CXXFLAGS += -I$(PAPI_INC)
LDFLAGS += -L$(PAPI_LIB)

RDMAX_INC = -I../rdmax/include
IBV_INC = -I/opt/ofed/include/ -I$(HOME)/libfab/include
IBV_LIB = -libverbs # -lfabric

JFCONTEXT = ../ult/fult/jump_x86_64_sysv_elf_gas.S
MFCONTEXT = ../ult/fult/make_x86_64_sysv_elf_gas.S

CXXFLAGS_NOBOOST = -I$(ABT_INC)

CXXFLAGS += $(IBV_INC)
LDFLAGS += $(IBV_LIB) # -llmpe -lmpe

DEPS = ../src/*.o mfcontext.o jfcontext.o ../*.h ../ult/*.h ../ult/fult/*.h ../ult/abt/*.h ../ult/pthread/*.h

all: pp jaco

jaco: jacobi_3d_mpiv jacobi_3d_mpiv_cond jacobi_3d_mpiv_abt

pp: pingpong pingpong_cond pingpong_abt \
		pingpong_mt pingpong_mt_pthread pingpong_mt_cond pingpong_mt_abt

abt_%: abt_%.c
	$(GXX) $(CXXFLAGS) $^ -o $@ -L$(ABT_LIB)

abx_%: abx_%.cxx
	$(MPICXX) $(CXXFLAGS_NOBOOST) $^ -o $@ -L$(ABT_LIB)

%: %.c
	make -C ../src
	$(MPICXX) $(CXXFLAGS) $(LDFLAGS) $^ ../src/libmv.a -o $@

%_abt: CXXFLAGS+=-DUSE_ABT
%_abt: $(DEPS) %.cxx
	$(MPICXX) $(CXXFLAGS) mfcontext.o jfcontext.o $*.cxx $(LDFLAGS) -o $@
	
%_cond: CXXFLAGS+=-DUSE_PTHREAD
%_cond: $(DEPS) %.cxx
	$(MPICXX) $(CXXFLAGS) mfcontext.o jfcontext.o $*.cxx $(LDFLAGS) -o $@

%_thread: CXXFLAGS+=-DUSE_PTHREAD
%_thread: $(DEPS) %_thread.cxx
	$(MPICXX) $(CXXFLAGS) mfcontext.o jfcontext.o $@.cxx $(LDFLAGS) -o $@

jacobi_3d: jacobi_3d.c
	$(MPICXX) -O3 $^ -o $@

clean:
	rm -rf $(TARGET) *.o
