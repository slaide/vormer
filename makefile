CC ?= gcc
CFLAGS = -std=gnu23 -Wall -Werror -Wpedantic -Wextra -Iinclude
LFLAGS = -lxcb -lxcb-xinput

OBJECTS = main.o system.o

all: main

main.o: src/main.c
	$(CC) $(CFLAGS) src/main.c -c -o main.o
system.o: src/system.c
	$(CC) $(CFLAGS) src/system.c -c -o system.o

main: $(OBJECTS)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJECTS) -o main

clean:
	rm -rf $(OBJECTS)