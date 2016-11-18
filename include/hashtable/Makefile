INCLUDE_DIR = ../../
CFLAGS ?= -Wall -std=c99 -O3 -g

hashtable.o: hashtable_arr.c
	$(CC) $(CFLAGS) -c $^ -I$(INCLUDE_DIR)/include -o $@

clean:
	rm *.o
