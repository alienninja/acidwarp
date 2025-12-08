CC = gcc
CFLAGS = -O2 -funroll-all-loops -std=c99
LDFLAGS = -lSDL2 -lGL -lGLEW -lm
SOURCES = acidwarp.c bit_map.c lut.c palinit.c rolnfade.c warp_text.c renderer_gl.c effects_rgb.c
OBJECTS = $(SOURCES:.c=.o)

acidwarp: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o acidwarp
	strip acidwarp

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f acidwarp $(OBJECTS)
