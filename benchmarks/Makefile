MPICC = mpicc
LD = mpicc
CC = gcc

CFLAGS += -g3 -ggdb -O3 -Wall -Wextra -std=gnu99 -I../include -DUSE_AFFI -D_GNU_SOURCE #-fopenmp
LDFLAGS += -O3 -Wl,-rpath=$(abspath ../) -Wl,-z,now -Wl,-z,relro -libverbs -lfabric #-L/work/03490/tg827895/hpcx/mxm/lib/ -lmxm 
LIB = ../libmv.a

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
ABT_INC = $(HOME)/argo/include
ABT_LIB = $(HOME)/argo/lib -labt
CFLAGS += -I$(ABT_INC)
LDFLAGS += -L$(ABT_LIB)

### FOR USING PAPI ##
# PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
# PAPI_INC = $(TACC_PAPI_INC)
# CFLAGS += -I$(PAPI_INC)
# LDFLAGS += -L$(PAPI_LIB)

%: %.c
	$(MPICC) $(CFLAGS) -c $^
	$(LD) $(LDFLAGS) $@.o $(LIB) -o $@

%_abt: %.c
	$(MPICC) $(CFLAGS) -c $^ -DUSE_ABT
	$(LD) $(LDFLAGS) $*.o $(LIB) -o $@

%_pth: %.c
	$(MPICC) $(CFLAGS) -c $^ -DUSE_PTH
	$(LD) $(LDFLAGS) $*.o $(LIB) -o $@

clean:
	rm -rf $(TARGET) *.o
