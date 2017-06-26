include ../config.mk

MPICC = mpicc 
LD = mpicc
CC = gcc
TARGET = tag queue

EXTRA = -DUSE_AFFI

CFLAGS += -g3 -ggdb -O3 -Wall -Wextra -std=gnu99 -pthread -I../include -I../ $(EXTRA) -D_GNU_SOURCE
LDFLAGS += -O3 -Wl,-rpath=$(abspath ../) -Wl,-z,now -Wl,-z,relro -pthread
LIB = ../liblwci.a

LDFLAGS += -lpsm2

CXXFLAGS = -g3 -O3 -std=c++11

all: $(TARGET)

%: %.c
	$(CC) $(CFLAGS) -c $^
	$(LD) $(LDFLAGS) $@.o $(LIB) -o $@

%_abt: %.c
	$(MPICC) $(CFLAGS) -c $^ -DUSE_ABT
	$(LD) $(LDFLAGS) $*.o $(LIB) -o $@

%_pth: %.c
	$(MPICC) $(CFLAGS) -c $^ -DUSE_PTH
	$(LD) $(LDFLAGS) $*.o $(LIB) -o $@

clean:
	rm -rf $(TARGET) *.o
