.PHONY: clean

CFLAGS = -g -W -Wall -std=c11 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lpng -ljpeg

TARGET = imv
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:%.c=%.o)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(TARGET) $(OBJECTS)
