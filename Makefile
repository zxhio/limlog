all:
	g++ -Og -g -Wall -o LogTest Log.cpp LogTest.cpp Timestamp.cpp -std=c++11 -lpthread

clean:
	rm -rf LogTest *.log