

CC = clang
CCPP = clang++
CFLAGS = `llvm-config --cflags`
CPPFLAGS = `llvm-config --cxxflags --ldflags` rembrandb.o `llvm-config --system-libs --libs`


binaries=rembrandb

rembrandb.o: database.c parser.h table.h Makefile
	$(CC) -g $(CFLAGS) -c database.c -O0 -o rembrandb.o
	$(CCPP) -g -std=c++11 $(CPPFLAGS) -o rembrandb

all: clean $(binaries)


clean:
	rm -f $(binaries) *.o