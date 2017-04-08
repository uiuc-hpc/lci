include ../config.mk

MPICC = $(HOME)/mpich-ch4/bin/mpicc
LD = mpicc
CC = gcc

CFLAGS += -g3 -ggdb -O3 -Wall -Wextra -std=gnu99 -pthread -I../include -I../ -DUSE_AFFI -D_GNU_SOURCE  -I/opt/ofed/include $(SERVER)
LDFLAGS += -O3 -Wl,-rpath=$(abspath ../) -Wl,-z,now -Wl,-z,relro -pthread
LIB = ../liblwci.a

CXXFLAGS = -g3 -O3 -std=c++11

ifeq ($(LC_SERVER), ofi)
	CFLAGS += -DLC_USE_SERVER_OFI -DAFF_DEBUG
	LDFLAGS += -lfabric # -L$(HOME)/libfab/lib -lfabric
endif

ifeq ($(LC_SERVER), ibv)
	CFLAGS += -DLC_USE_SERVER_IBV -DAFF_DEBUG
	LDFLAGS += -libverbs
	LDFLAGS += -L$(HOME)/tcmalloc_sb/lib -ltcmalloc
endif

ifeq ($(LC_SERVER), psm)
	CFLAGS += -DLC_USE_SERVER_PSM -DAFF_DEBUG
	LDFLAGS += -lpsm2 # -L$(HOME)/libfab/lib -lfabric
endif

### FOR TESTING HASH TABLE ###
#FACEBOOK_INC = -I$(HOME)/facebook/include -I$(HOME)/glog/include -I$(HOME)/folly/folly/double-conversion #-DNDEBUG
#FACEBOOK_LIB = -L$(HOME)/gflags/lib -lgflags -L$(HOME)/glog/lib -lglog -L$(HOME)/facebook/lib -lfolly -lpthread
#CFLAGS += $(FACEBOOK_INC)
#LDFLAGS += $(FACEBOOK_LIB)

### FOR TESTING LIBCUCKOO ###
CUCKOO_INC = $(HOME)/libcuckoo/include
CXXFLAGS += -I$(CUCKOO_INC)

### FOR TESTING TBB ###
# TBB_INC = -I/opt/apps/intel/15/composer_xe_2015.2.164/tbb/include/
# TBB_LIB = -ltbb -ltbbmalloc -L/opt/apps/intel/15/composer_xe_2015.2.164/tbb/lib/intel64/gcc4.1 -ltbb -ltbbmalloc
# LDFLAGS += $(TBB_LIB)
# CXXFLAGS += $(TBB_INC)

### FOR TESTING ARGOBOTS ###
ABT_INC = $(HOME)/argo/include
ABT_LIB = $(HOME)/argo/lib -labt
CFLAGS += -I$(ABT_INC)
LDFLAGS += -L$(ABT_LIB)

### FOR TESTING BOOST ###
#CXXFLAGS += -I$(TACC_BOOST_INC)
#LDFLAGS += -L$(TACC_BOOST_LIB)

### FOR USING PAPI ##
PAPI_LIB = $(TACC_PAPI_LIB) -lpfm -lpapi
PAPI_INC = $(TACC_PAPI_INC)
# CXXFLAGS += -I$(PAPI_INC)
#LDFLAGS += -L$(PAPI_LIB)

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

write_mpi: write_mpi.cxx
	mpicxx $(CXXFLAGS) write_mpi.cxx -o write_mpi

hash: hash.cxx
	mpicxx $(CXXFLAGS) $(LDFLAGS) -I../include -I../src/include/ hash.cxx -o hash ../src/hashtable.c

pool: pool.cxx
	mpicxx $(CXXFLAGS) $(LDFLAGS) -I../include -I../src/include/ pool.cxx -o pool  ../src/pool.c -DUSE_AFFI

packet: packet.cxx
	mpicxx $(CXXFLAGS) $(LDFLAGS) -I../include -I../src/include/ packet.cxx -o packet ../src/pool.c -DUSE_AFFI

lqueue: lqueue.cxx
	mpicxx $(CXXFLAGS) $(LDFLAGS) -I../include -I../src/include/ lqueue.cxx -o lqueue ../src/lcrq.c -DUSE_AFFI

pingpong_mt_NN_compute_: pingpong_mt_NN_compute.cxx
	mpicxx $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LIB) -I../include -I../src/include/ -DUSE_AFFI -DUSE_PAPI

clean:
	rm -rf $(TARGET) *.o
