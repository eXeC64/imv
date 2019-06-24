.PHONY: imv debug clean check install uninstall doc

include config.mk

PREFIX ?= /usr
BINPREFIX ?= $(PREFIX)/bin
MANPREFIX ?= $(PREFIX)/share/man
DATAPREFIX ?= $(PREFIX)/share
CONFIGPREFIX ?= /etc

INSTALL_DATA ?= install -m 0644
INSTALL_MAN ?= install -m 0644
INSTALL_PROGRAM ?= install -m 0755

override CFLAGS += -std=c99 -W -Wall -Wpedantic -Wextra
override CPPFLAGS += $(shell sdl2-config --cflags) -D_XOPEN_SOURCE=700
override LIBS := $(shell sdl2-config --libs)
override LIBS += -lSDL2_ttf -lfontconfig -lpthread

BUILDDIR ?= build
TARGET := $(BUILDDIR)/imv

SOURCES := src/main.c

SOURCES += src/binds.c
SOURCES += src/bitmap.c
SOURCES += src/commands.c
SOURCES += src/image.c
SOURCES += src/imv.c
SOURCES += src/ini.c
SOURCES += src/list.c
SOURCES += src/log.c
SOURCES += src/navigator.c
SOURCES += src/util.c
SOURCES += src/viewport.c

# Add backends to build as configured
ifeq ($(BACKEND_FREEIMAGE),yes)
	SOURCES += src/backend_freeimage.c
	override CPPFLAGS += -DIMV_BACKEND_FREEIMAGE
	override LIBS += -lfreeimage
endif

ifeq ($(BACKEND_LIBTIFF),yes)
	SOURCES += src/backend_libtiff.c
	override CPPFLAGS += -DIMV_BACKEND_LIBTIFF
	override LIBS += -ltiff
endif

ifeq ($(BACKEND_LIBPNG),yes)
	SOURCES += src/backend_libpng.c
	override CPPFLAGS += -DIMV_BACKEND_LIBPNG
	override LIBS += -lpng
endif

ifeq ($(BACKEND_LIBJPEG),yes)
	SOURCES += src/backend_libjpeg.c
	override CPPFLAGS += -DIMV_BACKEND_LIBJPEG
	override LIBS += -lturbojpeg
endif

ifeq ($(BACKEND_LIBRSVG),yes)
	SOURCES += src/backend_librsvg.c
	override CPPFLAGS += -DIMV_BACKEND_LIBRSVG $(shell pkg-config --cflags librsvg-2.0)
	override LIBS += $(shell pkg-config --libs librsvg-2.0)
endif


TEST_SOURCES := test/list.c test/navigator.c

OBJECTS := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))
TESTS := $(patsubst test/%.c,$(BUILDDIR)/test_%,$(TEST_SOURCES))

VERSION != git describe --dirty --always --tags 2> /dev/null || echo v3.1.1

override CPPFLAGS += -DIMV_VERSION=\""$(VERSION)"\"

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

$(BUILDDIR)/test_%: test/%.c $(filter-out src/main.c, $(SOURCES))
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
	mkdir -p $(DESTDIR)$(BINPREFIX)
	$(INSTALL_PROGRAM) $(TARGET) $(DESTDIR)$(BINPREFIX)/imv
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	$(INSTALL_MAN) doc/imv.1 $(DESTDIR)$(MANPREFIX)/man1/imv.1
	mkdir -p $(DESTDIR)$(MANPREFIX)/man5
	$(INSTALL_MAN) doc/imv.5 $(DESTDIR)$(MANPREFIX)/man5/imv.5
	mkdir -p $(DESTDIR)$(DATAPREFIX)/applications
	$(INSTALL_DATA) files/imv.desktop $(DESTDIR)$(DATAPREFIX)/applications/imv.desktop
	mkdir -p $(DESTDIR)$(CONFIGPREFIX)
	$(INSTALL_DATA) files/imv_config $(DESTDIR)$(CONFIGPREFIX)/imv_config

uninstall:
	$(RM) $(DESTDIR)$(BINPREFIX)/imv
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/imv.1
	$(RM) $(DESTDIR)$(MANPREFIX)/man5/imv.5
	$(RM) $(DESTDIR)$(DATAPREFIX)/applications/imv.desktop
	@echo "$(DESTDIR)$(CONFIGPREFIX)/imv_config has not been removed. Please remove it manually."
