CXX = g++
CXXFLAGS = -std=c++17 -march=native -O3 -Wall -Werror -I../include
LDFLAGS = -lpthread

SRCS = \
	ItoaTest.cpp \
	TimestampTest.cpp \
	BlockingBufferTest.cpp \
	Benchmark.cpp

OBJS = $(patsubst %.cpp, %.o, $(SRCS))
DEPS = $(patsubst %.cpp, %.d, $(SRCS))
TARGETS = $(patsubst %.cpp, %, $(SRCS))

all: $(TARGETS)

$(TARGETS): $(OBJS)
	$(CXX) $(CXXFLAGS) $@.o -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.d: %.cpp
	@ $(CXX) $(CXXFLAGS) -MM $< > $@

.PHONY: clean
clean:
	rm -rf *.log $(TARGETS) $(OBJS) $(DEPS)

-include $(DEPS)