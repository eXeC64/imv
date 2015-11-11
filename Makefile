.PHONY: clean install

prefix = /usr

CFLAGS = -W -Wall -std=gnu11 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lfreeimage

TARGET = imv
BUILDDIR = build

SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

debug: CFLAGS += -DDEBUG -g -pg
debug: $(TARGET)

$(BUILDDIR)/%.o: src/%.c $(BUILDDIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	$(RM) $(TARGET) $(OBJECTS)
	rmdir $(BUILDDIR)

install: $(TARGET)
	install -m 0755 $(TARGET) $(prefix)/bin
	install -m 0644 $(TARGET).1 $(prefix)/share/man/man1
