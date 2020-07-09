CXX	= g++
CXXFLAGS = -O3 -Wall -Werror -std=c++11 -I../include 
LDFLAGS = -lpthread

TARGET = LogTest

all:
	$(CXX) $(CXXFLAGS) -o $(TARGET) LogTest.cpp $(LDFLAGS)
	
clean:
	rm -rf *.log $(TARGET)