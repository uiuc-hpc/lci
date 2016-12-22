MPICC = mpicc
LD = mpicc
CC = gcc

CFLAGS += -g3 -ggdb -O3 -Wall -Wextra -std=gnu99 -I../include -DUSE_AFFI -D_GNU_SOURCE 
LDFLAGS += -O3 -Wl,-rpath=$(abspath ../) -Wl,-z,now -Wl,-z,relro -libverbs 
LDFLAGS += -lfabric
LIB = ../libmv.a

CXXFLAGS = -g3 -O3 -std=c++11

### FOR TESTING HASH TABLE ###
#FACEBOOK_INC = -I$(HOME)/facebook/include -I$(HOME)/glog/include -I$(HOME)/folly/folly/double-conversion #-DNDEBUG
#FACEBOOK_LIB = -L$(HOME)/gflags/lib -lgflags -L$(HOME)/glog/lib -lglog -L$(HOME)/facebook/lib -lfolly -lpthread
#CFLAGS += $(FACEBOOK_INC)
#LDFLAGS += $(FACEBOOK_LIB)

### FOR TESTING LIBCUCKOO ###
CUCKOO_INC = $(HOME)/libcuckoo/include
CXXFLAGS += -I$(CUCKOO_INC)

### FOR TESTING TBB ###
TBB_INC = #-I/opt/apps/intel/15/composer_xe_2015.2.164/tbb/include/
TBB_LIB = -ltbb -ltbbmalloc #-L/opt/apps/intel/15/composer_xe_2015.2.164/tbb/lib/intel64/gcc4.1 -ltbb -ltbbmalloc
LDFLAGS += $(TBB_LIB)
CXXFLAGS += $(TBB_INC)

### FOR TESTING ARGOBOTS ###
ABT_INC = $(HOME)/argo/include
ABT_LIB = $(HOME)/argo/lib -labt
CFLAGS += -I$(ABT_INC)
LDFLAGS += -L$(ABT_LIB)

### FOR TESTING BOOST ###
CXXFLAGS += -I$(TACC_BOOST_INC)
LDFLAGS += -L$(TACC_BOOST_LIB)

### FOR USING PAPI ##
# PAPI_LIB = $(TACC_PAPI_LIB) -lpapi
# PAPI_INC = $(TACC_PAPI_INC)
# CFLAGS += -I$(PAPI_INC)
# LDFLAGS += -L$(PAPI_LIB)

TARGET = pingpong_mt_NN pingpong_mt_NN_abt pingpong_mt_NN_pth pingpong_mt_NN_osu

all: $(TARGET)

%: %.c
	$(MPICC) $(CFLAGS) -c $^
	$(LD) $(LDFLAGS) $@.o $(LIB) -o $@

%_abt: %.c
	$(MPICC) $(CFLAGS) -c $^ -DUSE_ABT
	$(LD) $(LDFLAGS) $*.o $(LIB) -o $@

%_pth: %.c
	$(MPICC) $(CFLAGS) -c $^ -DUSE_PTH
	$(LD) $(LDFLAGS) $*.o $(LIB) -o $@

hash:
	mpicxx $(CXXFLAGS) $(LDFLAGS) -I../include -I../src/include/ hash.cxx -o hash ../src/hashtable.c

packet:
	mpicxx $(CXXFLAGS) $(LDFLAGS) -I../include -I../src/include/ packet.cxx -o packet ../src/pool.c -DUSE_AFFI

clean:
	rm -rf $(TARGET) *.o
