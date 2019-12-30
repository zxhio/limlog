
CXX = g++
CFLAGS = -std=c++11 -Wall -O3 -g -fno-common
LIBS = -L. -lpthread

LIB_SRCS = Timestamp.cpp Log.cpp NumToString.cpp
LIB_OBJS = $(LIB_SRCS:.cpp=.o)

TEST_SRCS = LogTest.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)
TEST_TARGET = LogTest

all: liblimlog.a $(TEST_TARGET)

%.o: %.cpp
	$(CXX) $(CFLAGS) -o $@ -c $<

liblimlog.a: $(LIB_OBJS)
	ar -cr liblimlog.a $(LIB_OBJS)

$(TEST_TARGET): $(TEST_OBJS) liblimlog.a
	$(CXX) $(CFLAGS) -o $@ $^ $(LIBS)

test:
	./$(TEST_TARGET)

clean:
	rm -rf $(TEST_OBJS) $(TEST_TARGET) $(LIB_OBJS) liblimlog.a *.log
