.PHONY: clean install

prefix = /usr

CFLAGS = -W -Wall -std=gnu11 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lfreeimage

TARGET = imv
BUILDDIR = build

SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))

$(TARGET): $(OBJECTS)
	@echo "LINKING $@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

debug: CFLAGS += -DDEBUG -g -pg
debug: $(TARGET)

$(BUILDDIR)/%.o: src/%.c
	@mkdir -p $(BUILDDIR)
	@echo "COMPILING $@"
	@$(CC) -c $(CFLAGS) -o $@ $<

clean:
	@$(RM) $(TARGET) $(OBJECTS)

install: $(TARGET)
	install -m 0755 $(TARGET) $(prefix)/bin
	install -m 0644 doc/imv.1 $(prefix)/share/man/man1
