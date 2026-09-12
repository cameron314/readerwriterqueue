ifeq ($(OS),Windows_NT)
	EXT=.exe
	PLATFORM_OPTS=-static
	PLATFORM_LD_OPTS=-pthread -Wl,--no-as-needed
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		EXT=
		PLATFORM_OPTS=
		PLATFORM_LD_OPTS=-pthread
	else ifeq ($(UNAME_S),Haiku)
		EXT=
		PLATFORM_OPTS=
		PLATFORM_LD_OPTS=-Wl,--no-as-needed
	else
		EXT=
		PLATFORM_OPTS=
		PLATFORM_LD_OPTS=-pthread -lrt -Wl,--no-as-needed
	endif
endif

default: stabtest$(EXT)

stabtest$(EXT): stabtest.cpp ../../readerwriterqueue.h ../../atomicops.h ../common/simplethread.h ../common/simplethread.cpp makefile
	g++ $(PLATFORM_OPTS) -std=c++11 -Wsign-conversion -Wpedantic -Wall -DNDEBUG -O3 stabtest.cpp ../common/simplethread.cpp -o stabtest$(EXT)  $(PLATFORM_LD_OPTS)

run: stabtest$(EXT)
	./stabtest$(EXT)
