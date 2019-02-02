.PHONY: imv debug clean check install uninstall doc

include config.mk

PREFIX ?= /usr
BINPREFIX ?= $(PREFIX)/bin
MANPREFIX ?= $(PREFIX)/share/man
DATAPREFIX ?= $(PREFIX)/share
CONFIGPREFIX ?= /etc

override CFLAGS += -std=c99 -W -Wall -Wpedantic -Wextra
override CPPFLAGS += $(shell sdl2-config --cflags) -D_XOPEN_SOURCE=700
override LIBS := $(shell sdl2-config --libs)
override LIBS += -lSDL2_ttf -lfontconfig -lpthread

BUILDDIR ?= build
TARGET := $(BUILDDIR)/imv

SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))
TESTS := $(patsubst test/%.c,$(BUILDDIR)/test_%,$(wildcard test/*.c))

VERSION != git describe --dirty --always --tags 2> /dev/null || echo v3.0.0

override CPPFLAGS += -DIMV_VERSION=\""$(VERSION)"\"

# Add backends to build as configured
ifeq ($(BACKEND_FREEIMAGE),yes)
	override CPPFLAGS += -DIMV_BACKEND_FREEIMAGE
	override LIBS += -lfreeimage
endif

ifeq ($(BACKEND_LIBTIFF),yes)
	override CPPFLAGS += -DIMV_BACKEND_LIBTIFF
	override LIBS += -ltiff
endif

ifeq ($(BACKEND_LIBPNG),yes)
	override CPPFLAGS += -DIMV_BACKEND_LIBPNG
	override LIBS += -lpng
endif

ifeq ($(BACKEND_LIBJPEG),yes)
	override CPPFLAGS += -DIMV_BACKEND_LIBJPEG
	override LIBS += -lturbojpeg
endif

ifeq ($(BACKEND_LIBRSVG),yes)
	override CPPFLAGS += -DIMV_BACKEND_LIBRSVG $(shell pkg-config --cflags librsvg-2.0)
	override LIBS += $(shell pkg-config --libs librsvg-2.0)
endif

TFLAGS ?= -g $(CFLAGS) $(CPPFLAGS) $(shell pkg-config --cflags cmocka)
TLIBS := $(LIBS) $(shell pkg-config --libs cmocka)

imv: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS)

debug: CFLAGS += -DDEBUG -g -pg
debug: $(TARGET)

$(OBJECTS): | $(BUILDDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: src/%.c Makefile
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

$(BUILDDIR)/test_%: test/%.c $(filter-out src/main.c, $(wildcard src/*.c))
	$(CC) -o $@ -Isrc $(TFLAGS) $^ $(LDFLAGS) $(TLIBS)

check: $(BUILDDIR) $(TESTS)
	for t in $(TESTS); do $$t; done

clean:
	$(RM) -Rf $(BUILDDIR)
	$(RM) doc/imv.1 doc/imv.5

doc: doc/imv.1 doc/imv.5

doc/%: doc/%.txt
	a2x --no-xmllint --doctype manpage --format manpage $<

install: $(TARGET) doc
	install -D -m 0755 $(TARGET) $(DESTDIR)$(BINPREFIX)/imv
	install -D -m 0644 doc/imv.1 $(DESTDIR)$(MANPREFIX)/man1/imv.1
	install -D -m 0644 doc/imv.5 $(DESTDIR)$(MANPREFIX)/man5/imv.5
	install -D -m 0644 files/imv.desktop $(DESTDIR)$(DATAPREFIX)/applications/imv.desktop
	install -D -m 0644 files/imv_config $(DESTDIR)$(CONFIGPREFIX)/imv_config

uninstall:
	$(RM) $(DESTDIR)$(BINPREFIX)/imv
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/imv.1
	$(RM) $(DESTDIR)$(MANPREFIX)/man5/imv.5
	$(RM) $(DESTDIR)$(DATAPREFIX)/applications/imv.desktop
	@echo "$(DESTDIR)$(CONFIGPREFIX)/imv_config has not been removed. Please remove it manually."
