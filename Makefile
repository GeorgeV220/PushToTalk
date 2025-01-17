CXX = g++
CXXFLAGS = -std=c++17 `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0`

all: main

main: main.o Settings.o InputHandler.o Utility.o
	$(CXX) -o main main.o Settings.o InputHandler.o Utility.o $(LDFLAGS)

main.o: src/main.cpp src/Settings.h src/InputHandler.h src/Utility.h
	$(CXX) $(CXXFLAGS) -c main.cpp

Settings.o: src/Settings.cpp src/Settings.h src/Utility.h
	$(CXX) $(CXXFLAGS) -c Settings.cpp

InputHandler.o: src/InputHandler.cpp src/InputHandler.h src/Settings.h src/Utility.h
	$(CXX) $(CXXFLAGS) -c InputHandler.cpp

Utility.o: src/Utility.cpp src/Utility.h
	$(CXX) $(CXXFLAGS) -c Utility.cpp

clean:
	rm -f *.o main
