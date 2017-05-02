CC = g++
DEBUG = -g -O0 -pedantic-errors
CFLAGS = -Wall -std=c++14 -c $(DEBUG)
LFLAGS = -Wall -lreadline $(DEBUG)

all: 1730sh

1730sh: 1730sh.o
	$(CC) $(LFLAGS) -o 1730sh 1730sh.o

1730sh.o: 1730sh.cpp
	$(CC) $(CFLAGS) 1730sh.cpp

.PHONY: clean
clean:
	rm -rf *.o
	rm -rf 1730sh
