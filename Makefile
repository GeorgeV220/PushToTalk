CXX = g++
CXXFLAGS = -std=c++17 `pkg-config --cflags gtk+-3.0` -I/usr/include/openal -I/usr/include/mpg123
LDFLAGS = `pkg-config --libs gtk+-3.0` -lopenal -lmpg123

all: main

main: main.o Settings.o InputHandler.o Utility.o
	$(CXX) -o main main.o Settings.o InputHandler.o Utility.o $(LDFLAGS)

main.o: src/main.cpp src/Settings.h src/InputHandler.h src/Utility.h
	$(CXX) $(CXXFLAGS) -c src/main.cpp

Settings.o: src/Settings.cpp src/Settings.h src/Utility.h
	$(CXX) $(CXXFLAGS) -c src/Settings.cpp

InputHandler.o: src/InputHandler.cpp src/InputHandler.h src/Settings.h src/Utility.h
	$(CXX) $(CXXFLAGS) -c src/InputHandler.cpp

Utility.o: src/Utility.cpp src/Utility.h
	$(CXX) $(CXXFLAGS) -c src/Utility.cpp

clean:
	rm -f *.o main
