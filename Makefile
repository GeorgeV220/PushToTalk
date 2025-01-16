CXX = g++
CXXFLAGS = -std=c++17 `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0`

all: main

main: main.o Settings.o InputHandler.o Utility.o
	$(CXX) -o main main.o Settings.o InputHandler.o Utility.o $(LDFLAGS)

main.o: main.cpp Settings.h InputHandler.h Utility.h
	$(CXX) $(CXXFLAGS) -c main.cpp

Settings.o: Settings.cpp Settings.h Utility.h
	$(CXX) $(CXXFLAGS) -c Settings.cpp

InputHandler.o: InputHandler.cpp InputHandler.h Settings.h Utility.h
	$(CXX) $(CXXFLAGS) -c InputHandler.cpp

Utility.o: Utility.cpp Utility.h
	$(CXX) $(CXXFLAGS) -c Utility.cpp

clean:
	rm -f *.o main
