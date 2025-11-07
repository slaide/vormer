CC ?= gcc
CFLAGS = -std=gnu23 -Wall -Werror -Wpedantic -Wextra -Iinclude -g
LFLAGS = -lm -lxcb -lxcb-xinput -lvulkan

OBJECTS = main.o system.o scene.o
SHADERS = resources/shader.vert.spv resources/shader.frag.spv

APPNAME = main

all: $(APPNAME) $(OBJECTS) $(SHADERS)

# help:
# $^ <- all inputs
# $@ <- output file
# $< <- first (leftmost) input
# % wildcard

# vertex shaders
%.vert.spv: %.vert.glsl
	glslc -fshader-stage=vert $< -o $@
# fragment shaders
%.frag.spv: %.frag.glsl
	glslc -fshader-stage=frag $< -o $@

%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $^

$(APPNAME): $(OBJECTS)
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@

clean:
	$(RM) $(APPNAME) $(OBJECTS) $(SHADERS)
