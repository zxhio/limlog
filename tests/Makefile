CXX = g++
CXXFLAGS = -std=c++11 -march=native -O3 -Wall -Werror -I../include
LDFLAGS = -lpthread

SRCS =  ItoaTest.cpp \
	TimestampTest.cpp \
	BlockingBufferTest.cpp \
	Benchmark.cpp

TARGETS = $(patsubst %.cpp, %, $(SRCS))

all: $(TARGETS)

$(TARGETS):
	$(CXX) $(CXXFLAGS) $@.cpp -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf *.log $(TARGETS)