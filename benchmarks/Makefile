include ../config.mk

MPICC = mpicc
LD = mpicc
CC = gcc
CXX = g++

EXTRA = -DUSE_AFFI

CFLAGS += -g3 -ggdb -O3 -Wall -Wextra -std=gnu99 -pthread -I../include -I../ $(EXTRA) -D_GNU_SOURCE  -I/opt/ofed/include $(SERVER)
LDFLAGS += -O3 -Wl,-rpath=$(abspath ../) -Wl,-z,now -Wl,-z,relro -pthread
LIB = ../liblwci.a

CXXFLAGS = -g3 -O3 -std=c++11 -D_GNU_SOURCE $(EXTRA)

ifeq ($(LC_SERVER), ofi)
	CFLAGS += -DLC_USE_SERVER_OFI
	LDFLAGS += -lfabric # -L$(HOME)/libfab/lib -lfabric
endif

ifeq ($(LC_SERVER), ibv)
	CFLAGS += -DLC_USE_SERVER_IBV
	LDFLAGS += -libverbs
	# LDFLAGS += -L$(HOME)/tcmalloc_sb/lib -ltcmalloc
endif

ifeq ($(LC_SERVER), psm)
	CFLAGS += -DLC_USE_SERVER_PSM
	LDFLAGS += -lpsm2 # -L$(HOME)/libfab/lib -lfabric
endif

### FOR TESTING HASH TABLE ###
#FACEBOOK_INC = -I$(HOME)/facebook/include -I$(HOME)/glog/include -I$(HOME)/folly/folly/double-conversion #-DNDEBUG
#FACEBOOK_LIB = -L$(HOME)/gflags/lib -lgflags -L$(HOME)/glog/lib -lglog -L$(HOME)/facebook/lib -lfolly -lpthread
#CFLAGS += $(FACEBOOK_INC)
#LDFLAGS += $(FACEBOOK_LIB)

### FOR TESTING LIBCUCKOO ###
CUCKOO_INC = -I$(HOME)/libcuckoo/include

### FOR TESTING TBB ###
TBB_INC = -I$(TACC_INTEL_DIR)/tbb/include
TBB_LIB = -L$(TACC_INTEL_DIR)/tbb/lib/intel64/gcc4.4/ -ltbb -ltbbmalloc

### FOR TESTING ARGOBOTS ###
ABT_INC = $(HOME)/argo/include
ABT_LIB = $(HOME)/argo/lib -labt

### FOR TESTING BOOST ###
CXXFLAGS += -I$(TACC_BOOST_INC)
LDFLAGS += -L$(TACC_BOOST_LIB)

### FOR USING PAPI ##
PAPI_LIB = $(TACC_PAPI_LIB) -lpfm -lpapi
PAPI_INC = $(TACC_PAPI_INC)
CXXFLAGS += -I$(PAPI_INC)
LDFLAGS += -L$(PAPI_LIB)

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
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TBB_INC) $(TBB_LIB) $(CUCKOO_INC) -I../include -I../src/include/ hash.cxx -o hash ../src/hashtable.c ../liblwci.a

hash_c: hash.cxx
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TBB_INC) $(TBB_LIB) $(CUCKOO_INC) -I../include -I../src/include/ hash.cxx -o hash_c ../src/hashtable.c ../liblwci.a -DMEASURE_CACHE -DPAPI_COUNTER=PAPI_L1_DCM

hash_i: hash.cxx
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TBB_INC) $(TBB_LIB) $(CUCKOO_INC) -I../include -I../src/include/ hash.cxx -o hash_i ../src/hashtable.c ../liblwci.a -DMEASURE_CACHE -DPAPI_COUNTER=PAPI_TOT_INS

pool: pool.cxx
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -I../include -I../src/include/ pool.cxx -o pool  ../src/pool.c -DUSE_AFFI

packet: packet.cxx
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -I../include -I../src/include/ packet.cxx -o packet ../src/pool.c -DUSE_AFFI

lqueue: lqueue.cxx
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -I../include -I../src/include/ lqueue.cxx -o lqueue ../src/lcrq.c -DUSE_AFFI

pingpong_mt_NN_compute_: pingpong_mt_NN_compute.cxx
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LIB) -I../include -I../src/include/ -DUSE_AFFI -DUSE_PAPI

clean:
	rm -rf $(TARGET) *.o
