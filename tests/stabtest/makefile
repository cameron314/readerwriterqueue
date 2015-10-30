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


default: stabtest$(EXT)

stabtest$(EXT): stabtest.cpp ../../readerwriterqueue.h ../../atomicops.h ../common/simplethread.h ../common/simplethread.cpp makefile
	g++ $(PLATFORM_OPTS) -std=c++11 -Wpedantic -Wall -DNDEBUG -O3 stabtest.cpp ../common/simplethread.cpp -o stabtest$(EXT) -pthread

run: stabtest$(EXT)
	./stabtest$(EXT)
