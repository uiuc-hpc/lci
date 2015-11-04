TARGET = yield wait abt_yield abt_wait fibo abt_fibo fair comm_order comm_reorder \
    comm comm_queue abx_comm abx_comm2 abx_comm_order abx_comm_reorder \
    abt_fair abx_comm2_reorder abx_comm2_order

GXX = mpicxx
CXXFLAGS = -g3 -O3 -Wall -Wextra -I../
LDFLAGS = -lboost_context -lboost_coroutine -lpthread -labt -lm

BOOST_INC = /usr/include/boost
BOOST_LIB = /usr/lib/boost

all: $(TARGET)

abt_%: abt_%.c
	gcc $(CXXFLAGS) $^ -o $@ -labt -lm

%: %.cxx
	$(GXX) $(CXXFLAGS) -std=c++11 $^ -o $@ -I$(BOOST_INC) -L$(BOOST_LIB) $(LDFLAGS)

clean:
	rm -rf $(TARGET)
