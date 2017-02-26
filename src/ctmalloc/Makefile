all: malloc.o PageAllocator.o  PartitionAlloc.o

%.o: %.cpp
	g++ -DNDEBUG -fPIC -O3 -g3 -c $^ -o $@

clean:
	rm -rf *.o *.a
