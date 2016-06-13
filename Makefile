

CC = clang++
CFLAGS = `llvm-config --cxxflags --ldflags --system-libs --libs`


rembrandb.o: database.cpp
	$(CC) -g database.cpp $(CFLAGS) -O0 -o rembrandb 