.PHONY: imv clean check install uninstall

PREFIX ?= /usr
BINPREFIX ?= $(PREFIX)/bin
MANPREFIX ?= $(PREFIX)/share/man
DATAPREFIX ?= $(PREFIX)/share

ifeq ($(V),)
MUTE :=	@
endif

CFLAGS ?= -W -Wall -pedantic -Wmissing-prototypes
CFLAGS += -std=c99 $(shell sdl2-config --cflags) -D_XOPEN_SOURCE=700
LDFLAGS += $(shell sdl2-config --libs) -lfreeimage -lSDL2_ttf -lfontconfig -lpthread

BUILDDIR ?= build
TARGET := $(BUILDDIR)/imv

SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))
TESTS := $(patsubst test/%.c,$(BUILDDIR)/test_%,$(wildcard test/*.c))
TFLAGS ?= -g ${CFLAGS}

VERSION := $(shell git describe --abbrev=8 --dirty --always --tags 2> /dev/null)
ifeq ($(VERSION),)
VERSION := v2.1.3
endif

CFLAGS += -DIMV_VERSION=\""$(VERSION)"\"

imv: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "LINKING $@"
	$(MUTE)$(CC) -o $@ $^ $(LDLIBS) $(LDFLAGS)

debug: CFLAGS += -DDEBUG -g -pg
debug: $(TARGET)

$(OBJECTS): | $(BUILDDIR)

$(BUILDDIR):
	$(MUTE)mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: src/%.c
	@echo "COMPILING $@"
	$(MUTE)$(CC) -c $(CFLAGS) -o $@ $<

$(BUILDDIR)/test_%: test/%.c src/%.c
	@echo "BUILDING $@"
	$(MUTE)$(CC) -o $@ -Isrc $(TFLAGS) $^ $(LDFLAGS) -lcmocka

check: $(BUILDDIR) $(TESTS)
	@echo "RUNNING TESTS"
	$(MUTE)for t in $(TESTS); do $$t; done

clean:
	$(MUTE)$(RM) -Rf $(BUILDDIR)

install: $(TARGET)
	install -D -m 0755 $(TARGET) $(DESTDIR)$(BINPREFIX)/imv
	install -D -m 0644 doc/imv.1 $(DESTDIR)$(MANPREFIX)/man1/imv.1
	install -D -m 0644 files/imv.desktop $(DESTDIR)$(DATAPREFIX)/applications/imv.desktop

uninstall:
	$(RM) $(DESTDIR)$(BINPREFIX)/imv
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/imv.1
	$(RM) $(DESTDIR)$(DATAPREFIX)/applications/imv.desktop
