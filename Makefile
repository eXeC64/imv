.PHONY: clean install

prefix = /usr

CFLAGS = -W -Wall -std=gnu11 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lfreeimage

TARGET = imv
BUILDDIR = build

SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))

VERSION = "v1.1.0"

CFLAGS += -DIMV_VERSION=\"$(VERSION)\"

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
	install -D -m 0755 $(TARGET) $(DESTDIR)$(prefix)/bin/$(TARGET)
	install -D -m 0644 doc/imv.1 $(DESTDIR)$(prefix)/share/man/man1/imv.1
	install -D -m 0644 files/imv.desktop $(DESTDIR)$(prefix)/share/applications/imv.desktop
