

ifeq ($(OS),Windows_NT)
	EXT=.exe
	PLATFORM_OPTS=-static
else
	EXT=
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		PLATFORM_OPTS=
	else
		PLATFORM_OPTS=-Wl,--no-as-needed
	endif
endif


default: unittests$(EXT)

unittests$(EXT): unittests.cpp ../../readerwriterqueue.h ../../atomicops.h ../common/simplethread.h ../common/simplethread.cpp minitest.h makefile
	g++ $(PLATFORM_OPTS) -std=c++11 -Wpedantic -Wall -DNDEBUG -O3 -g unittests.cpp ../common/simplethread.cpp -o unittests$(EXT) -pthread

run: unittests$(EXT)
	./unittests$(EXT)
