MPICC = mpicc
GXX = gcc

CFLAGS += -g -ggdb -O3 -Wall -Wextra -std=gnu99 -I../ult -I../include -I../
CFLAGS += -D_GNU_SOURCE -pthread -DUSE_AFFI

### FOR TESTING HASH TABLE ###
#FACEBOOK_INC = -I$(HOME)/facebook/include -I$(HOME)/glog/include -I$(HOME)/folly/folly/double-conversion #-DNDEBUG
#FACEBOOK_LIB = -L$(HOME)/gflags/lib -lgflags -L$(HOME)/glog/lib -lglog -L$(HOME)/facebook/lib -lfolly -lpthread
#CFLAGS += $(FACEBOOK_INC)
#LDFLAGS += $(FACEBOOK_LIB)

### FOR TESTING LIBCUCKOO ###
# CUCKOO_INC = $(HOME)/libcuckoo/include
# CFLAGS += -I$(CUCKOO_INC)

### FOR TESTING TBB ###
# TBB_INC = -I/opt/apps/intel/15/composer_xe_2015.2.164/tbb/include/
# TBB_LIB = -L/opt/apps/intel/15/composer_xe_2015.2.164/tbb/lib/intel64/gcc4.1 -ltbb -ltbbmalloc
# LDFLAGS += $(TBB_LIB)
# CFLAGS += $(TBB_INC)

### FOR TESTING ARGOBOTS ###
# ABT_INC = $(HOME)/argo/include
# ABT_LIB = $(HOME)/argo/lib -labt
# CFLAGS += -I$(ABT_INC)
# LDFLAGS += -L$(ABT_LIB)

### FOR USING PAPI ##
PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
PAPI_INC = $(TACC_PAPI_INC)
CFLAGS += -I$(PAPI_INC)
LDFLAGS += -L$(PAPI_LIB)

RDMAX_INC = -I../rdmax/include
IBV_INC = -I/opt/ofed/include/ -I$(HOME)/libfab/include
IBV_LIB = -libverbs # -lfabric

CFLAGS += $(IBV_INC)
LDFLAGS += $(IBV_LIB) # -llmpe -lmpe

DEPS = ../src/*.o mfcontext.o jfcontext.o ../*.h ../ult/*.h ../ult/fult/*.h ../ult/abt/*.h ../ult/pthread/*.h

all: pp jaco

jaco: jacobi_3d_mpiv jacobi_3d_mpiv_cond jacobi_3d_mpiv_abt

pp: pingpong pingpong_cond pingpong_abt \
		pingpong_mt pingpong_mt_pthread pingpong_mt_cond pingpong_mt_abt

abt_%: abt_%.c
	$(GXX) $(CFLAGS) $^ -o $@ -L$(ABT_LIB)

abx_%: abx_%.cxx
	$(MPICC) $(CFLAGS_NOBOOST) $^ -o $@ -L$(ABT_LIB)

%: %.c
	make -C ../src
	$(MPICC) $(CFLAGS) $(LDFLAGS) $^ ../src/libmv.a -o $@

%_abt: CFLAGS+=-DUSE_ABT
%_abt: $(DEPS) %.cxx
	$(MPICC) $(CFLAGS) mfcontext.o jfcontext.o $*.cxx $(LDFLAGS) -o $@
	
%_cond: CFLAGS+=-DUSE_PTHREAD
%_cond: $(DEPS) %.cxx
	$(MPICC) $(CFLAGS) mfcontext.o jfcontext.o $*.cxx $(LDFLAGS) -o $@

%_thread: CFLAGS+=-DUSE_PTHREAD
%_thread: $(DEPS) %_thread.cxx
	$(MPICC) $(CFLAGS) mfcontext.o jfcontext.o $@.cxx $(LDFLAGS) -o $@

jacobi_3d: jacobi_3d.c
	$(MPICC) -O3 $^ -o $@

clean:
	rm -rf $(TARGET) *.o
