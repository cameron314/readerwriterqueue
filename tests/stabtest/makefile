ifeq ($(OS),Windows_NT)
	EXT=.exe
	PLATFORM_OPTS=-static
else
	EXT=
	PLATFORM_OPTS=
endif


default: stabtest$(EXT)

stabtest$(EXT): stabtest.cpp ../../readerwriterqueue.h ../../atomicops.h ../common/simplethread.h ../common/simplethread.cpp makefile
	g++ $(PLATFORM_OPTS) -std=c++11 -Wpedantic -Wall -DNDEBUG -O3 stabtest.cpp ../common/simplethread.cpp -o stabtest$(EXT) -pthread -Wl,--no-as-needed

run: stabtest$(EXT)
	./stabtest$(EXT)
