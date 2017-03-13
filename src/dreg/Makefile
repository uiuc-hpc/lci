CFLAGS = -fPIC -O3 -Wall -Wextra -D_GNU_SOURCE -pthread

all: test

test: dreg.o reg.o avl.o test.c
	mpicc -fopenmp $(CFLAGS) $^ -o $@ -I./

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -I./

clean:
	rm -rf *.o
