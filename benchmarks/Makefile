TARGET = yield wait abt_yield abt_wait fibo abt_fibo fair comm_order comm_reorder \
    comm comm_queue abx_comm abx_comm2 abx_comm_order abx_comm_reorder \
    abt_fair abx_comm2_reorder abx_comm2_order

GXX = mpicxx
CXXFLAGS = -g3 -O3 -Wall -Wextra -I../fult -I../

LDFLAGS = -lpthread -lm

CUCKOO_INC = $(HOME)/Softwares/libcuckoo/include

BOOST_INC = /usr/local/boost/1.58.0/include
BOOST_LIB = /usr/local/boost/1.58.0/lib -lboost_context \
	-lboost_coroutine -lboost_thread -lboost_system -lboost_chrono

ABT_INC = $(HOME)/Softwares/argo/include
ABT_LIB = $(HOME)/Softwares/argo/lib -labt

CXXFLAGS += -I$(BOOST_INC) -I$(ABT_INC) -I$(CUCKOO_INC)
LDFLAGS += -L$(BOOST_LIB) -L$(ABT_LIB)

all: $(TARGET)

abt_%: abt_%.c
	gcc $(CXXFLAGS) $^ -o $@ -L$(ABT_LIB) -labt -lm

%: %.cxx
	$(GXX) $(CXXFLAGS) -std=c++11 $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(TARGET)
