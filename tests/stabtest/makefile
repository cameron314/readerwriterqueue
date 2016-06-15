ifeq ($(OS),Windows_NT)
	EXT=.exe
	PLATFORM_OPTS=-static
	PLATFORM_LD_OPTS=
else
	EXT=
	PLATFORM_OPTS=
	PLATFORM_LD_OPTS=-lrt
endif


default: stabtest$(EXT)

stabtest$(EXT): stabtest.cpp ../../readerwriterqueue.h ../../atomicops.h ../common/simplethread.h ../common/simplethread.cpp makefile
	g++ $(PLATFORM_OPTS) -std=c++11 -Wpedantic -Wall -DNDEBUG -O3 stabtest.cpp ../common/simplethread.cpp -o stabtest$(EXT) -pthread $(PLATFORM_LD_OPTS) -Wl,--no-as-needed

run: stabtest$(EXT)
	./stabtest$(EXT)
