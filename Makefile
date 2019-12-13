all:
	g++ -Og -o LogTest Log.cpp LogTest.cpp -std=c++11 -lpthread

clean:
	rm -rf LogTest ./limlog