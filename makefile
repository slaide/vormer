CC ?= gcc
CFLAGS = -std=gnu23 -Wall -Werror -Wpedantic -Wextra -Iinclude -g
LFLAGS = -lxcb -lxcb-xinput -lvulkan

OBJECTS = main.o system.o

all: main resources/shader.vert.spv resources/shader.frag.spv

resources/shader.vert.spv: resources/shader.vert.glsl
	glslc -fshader-stage=vert resources/shader.vert.glsl -o resources/shader.vert.spv

resources/shader.frag.spv: resources/shader.frag.glsl
	glslc -fshader-stage=frag resources/shader.frag.glsl -o resources/shader.frag.spv

main.o: src/main.c
	$(CC) $(CFLAGS) src/main.c -c -o main.o
system.o: src/system.c
	$(CC) $(CFLAGS) src/system.c -c -o system.o

main: $(OBJECTS)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJECTS) -o main

clean:
	rm -rf $(OBJECTS)