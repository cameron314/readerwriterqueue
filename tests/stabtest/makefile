ifeq ($(OS),Windows_NT)
	EXT = .exe
else
	EXT =
endif


default: stabtest$(EXT)

stabtest$(EXT): stabtest.cpp ../../readerwriterqueue.h ../../atomicops.h ../common/simplethread.h ../common/simplethread.cpp makefile
	g++ -std=c++11 -pthread -DNDEBUG -O3 stabtest.cpp ../common/simplethread.cpp -o stabtest$(EXT)

run: stabtest$(EXT)
	./stabtest$(EXT)
