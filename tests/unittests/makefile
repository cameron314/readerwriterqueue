ifeq ($(OS),Windows_NT)
	EXT=.exe
	PLATFORM_OPTS=-static
	PLATFORM_LD_OPTS=
else
	EXT=
	PLATFORM_OPTS=
	PLATFORM_LD_OPTS=-lrt
endif


default: unittests$(EXT)

unittests$(EXT): unittests.cpp ../../readerwriterqueue.h ../../atomicops.h ../common/simplethread.h ../common/simplethread.cpp minitest.h makefile
	g++ $(PLATFORM_OPTS) -std=c++11 -Wpedantic -Wall -DNDEBUG -O3 -g unittests.cpp ../common/simplethread.cpp -o unittests$(EXT) -pthread $(PLATFORM_LD_OPTS) -Wl,--no-as-needed

run: unittests$(EXT)
	./unittests$(EXT)
