

CC = clang
CCPP = clang++
CFLAGS = `llvm-config --cflags`
CPPFLAGS = `llvm-config --cxxflags --ldflags`
CPPLIBS =  `llvm-config --system-libs --libs` -L`pwd` -lLLVMTargetMachineExtra


binaries=rembrandb libLLVMTargetMachineExtra.a llvmtest

all: rembrandb.o

libLLVMTargetMachineExtra.a: target_machine.h target_machine.cpp
	$(CCPP) -std=c++11 $(CFLAGS) -c target_machine.cpp  -O3 -o target_machine.o
	ar rs libLLVMTargetMachineExtra.a  target_machine.o

rembrandb.o: database.c parser.h table.h Makefile target_machine.h target_machine.cpp libLLVMTargetMachineExtra.a
	$(CC) -g $(CFLAGS) -c database.c -O0 -o rembrandb.o
	$(CCPP) -g -std=c++11 $(CPPFLAGS) rembrandb.o $(CPPLIBS) -o rembrandb

llvmtest.o: llvmtest.c target_machine.h target_machine.cpp libLLVMTargetMachineExtra.a
	$(CC) $(CFLAGS) -c llvmtest.c -O3 -o llvmtest.o
	$(CCPP) -std=c++11 $(CPPFLAGS) llvmtest.o $(CPPLIBS) -o llvmtest

clean:
	rm -f $(binaries) *.o