# ©2014 Cameron Desrochers

ifeq ($(OS),Windows_NT)
	EXT = .exe
	PLATFORM_OPTS = -static
else
	EXT =
	PLATFORM_OPTS = -lrt
endif

default: benchmarks$(EXT)

benchmarks$(EXT): bench.cpp ../readerwriterqueue.h ../atomicops.h ext/1024cores/spscqueue.h ext/folly/ProducerConsumerQueue.h ../tests/common/simplethread.h ../tests/common/simplethread.cpp systemtime.h systemtime.cpp makefile
	g++ -std=c++11 -Wpedantic -Wall -DNDEBUG -O3 -g $(PLATFORM_OPTS) bench.cpp ../tests/common/simplethread.cpp systemtime.cpp -o benchmarks$(EXT) -pthread -Wl,--no-as-needed

run: benchmarks$(EXT)
	./benchmarks$(EXT)
