.PHONY: clean check install uninstall

PREFIX ?= /usr
BINPREFIX ?= $(PREFIX)/bin
MANPREFIX ?= $(PREFIX)/share/man
DATAPREFIX ?= $(PREFIX)/share

V ?=

CFLAGS ?= -W -Wall -Wpedantic
CFLAGS += -std=gnu11 $(shell sdl2-config --cflags)
LDFLAGS += $(shell sdl2-config --libs) -lfreeimage -lSDL2_ttf -lfontconfig -lpthread

TARGET = imv
BUILDDIR = build

SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))
TESTS = $(patsubst test/%.c,test_%,$(wildcard test/*.c))

VERSION = "v1.2.0"

CFLAGS += -DIMV_VERSION=\"$(VERSION)\"

$(TARGET): $(OBJECTS)
	@echo "LINKING $@"
	$(V:%=@)$(CC) -o $@ $^ $(LDLIBS) $(LDFLAGS)

debug: CFLAGS += -DDEBUG -g -pg
debug: $(TARGET)

$(BUILDDIR)/%.o: src/%.c
	$(V:%=@)mkdir -p $(BUILDDIR)
	@echo "COMPILING $@"
	$(V:%=@)$(CC) -c $(CFLAGS) -o $@ $<

test_%: test/%.c src/%.c
	@echo "BUILDING $@"
	$(V:%=@)$(CC) -o $@ -Isrc -W -Wall -std=gnu11 -lcmocka $^

check: $(TESTS)
	@echo "RUNNING TESTS"
	$(V:%=@)for t in "$(TESTS)"; do ./$$t; done

clean:
	$(V:%=@)$(RM) $(TARGET) $(OBJECTS) $(TESTS)

install: $(TARGET)
	install -D -m 0755 $(TARGET) $(DESTDIR)$(BINPREFIX)/imv
	install -D -m 0644 doc/imv.1 $(DESTDIR)$(MANPREFIX)/man1/imv.1
	install -D -m 0644 files/imv.desktop $(DESTDIR)$(DATAPREFIX)/applications/imv.desktop

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/imv
	$(RM) $(DESTDIR)$(PREFIX)/share/man/man1/imv.1
	$(RM) $(DESTDIR)$(PREFIX)/share/applications/imv.desktop
