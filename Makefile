.PHONY: all imv debug clean check install uninstall doc

include config.mk

PREFIX ?= /usr
BINPREFIX ?= $(PREFIX)/bin
MANPREFIX ?= $(PREFIX)/share/man
DATAPREFIX ?= $(PREFIX)/share
CONFIGPREFIX ?= /etc

INSTALL_DATA ?= install -m 0644
INSTALL_MAN ?= install -m 0644
INSTALL_PROGRAM ?= install -m 0755
INSTALL_SCRIPT ?= install -m 0755

override CFLAGS += -std=c99 -W -Wall -Wpedantic -Wextra $(shell pkg-config --cflags pangocairo)
override CPPFLAGS += -D_XOPEN_SOURCE=700
override LIBS := -lGL -lpthread -lxkbcommon $(shell pkg-config --libs pangocairo) $(shell pkg-config --libs icu-io)

BUILDDIR ?= build
TARGET_WAYLAND = $(BUILDDIR)/imv-wayland
TARGET_X11 = $(BUILDDIR)/imv-x11
TARGET_MSG = $(BUILDDIR)/imv-msg

ifeq ($(WINDOWS),wayland)
	TARGETS := $(TARGET_WAYLAND) $(TARGET_MSG)
else ifeq ($(WINDOWS),x11)
	TARGETS := $(TARGET_X11) $(TARGET_MSG)
else ifeq ($(WINDOWS),all)
	TARGETS := $(TARGET_WAYLAND) $(TARGET_X11) $(TARGET_MSG)
endif

SOURCES := src/main.c

SOURCES += src/binds.c
SOURCES += src/bitmap.c
SOURCES += src/canvas.c
SOURCES += src/commands.c
SOURCES += src/console.c
SOURCES += src/image.c
SOURCES += src/imv.c
SOURCES += src/ini.c
SOURCES += src/ipc.c
SOURCES += src/ipc_common.c
SOURCES += src/keyboard.c
SOURCES += src/list.c
SOURCES += src/log.c
SOURCES += src/navigator.c
SOURCES += src/source.c
SOURCES += src/viewport.c

WL_SOURCES = src/wl_window.c src/xdg-shell-protocol.c
WL_LIBS = -lwayland-client -lwayland-egl -lEGL -lrt

X11_SOURCES = src/x11_window.c
X11_LIBS = -lX11 -lGL -lGLU -lxcb -lxkbcommon-x11

MSG_SOURCES = src/imv_msg.c src/ipc_common.c
MSG_LIBS =

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

ifeq ($(BACKEND_LIBNSGIF),yes)
	SOURCES += src/backend_libnsgif.c
	override CPPFLAGS += -DIMV_BACKEND_LIBNSGIF $(shell pkg-config --cflags libnsgif)
	override LIBS += $(shell pkg-config --libs libnsgif)
endif

# Only add inotify support on linux
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	SOURCES += src/reload.c
endif

TEST_SOURCES := test/list.c test/navigator.c

OBJECTS := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))
WL_OBJECTS := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(WL_SOURCES))
X11_OBJECTS := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(X11_SOURCES))
MSG_OBJECTS := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(MSG_SOURCES))

TESTS := $(patsubst test/%.c,$(BUILDDIR)/test_%,$(TEST_SOURCES))

VERSION != git describe --dirty --always --tags 2> /dev/null || echo v4.0.1

override CPPFLAGS += -DIMV_VERSION=\""$(VERSION)"\"

TFLAGS ?= -g $(CFLAGS) $(CPPFLAGS) $(shell pkg-config --cflags cmocka)
TLIBS := $(LIBS) $(shell pkg-config --libs cmocka)

all: imv doc

imv: $(TARGETS)

$(TARGET_WAYLAND): $(OBJECTS) $(WL_OBJECTS)
	$(CC) -o $@ $^ $(LIBS) $(WL_LIBS) $(LDFLAGS)

$(TARGET_X11): $(OBJECTS) $(X11_OBJECTS)
	$(CC) -o $@ $^ $(LIBS) $(X11_LIBS) $(LDFLAGS)

$(TARGET_MSG): $(MSG_OBJECTS)
	$(CC) -o $@ $^ $(MSG_LIBS) $(LDFLAGS)

debug: CFLAGS += -DDEBUG -g -pg
debug: $(TARGETS)

$(OBJECTS): | $(BUILDDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: src/%.c Makefile
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

$(BUILDDIR)/test_%: test/%.c src/dummy_window.c $(filter-out src/main.c, $(SOURCES))
	$(CC) -o $@ -Isrc $(TFLAGS) $^ $(LDFLAGS) $(TLIBS)

check: $(BUILDDIR) $(TESTS)
	for t in $(TESTS); do $$t; done

clean:
	$(RM) -Rf $(BUILDDIR)
	$(RM) doc/imv.1 doc/imv-msg.1 doc/imv.5

doc: doc/imv.1 doc/imv-msg.1 doc/imv.5

doc/%: doc/%.txt
	a2x --no-xmllint --doctype manpage --format manpage $<

install: $(TARGETS) doc
	mkdir -p $(DESTDIR)$(BINPREFIX)
ifeq ($(WINDOWS),wayland)
		$(INSTALL_PROGRAM) $(TARGET_WAYLAND) $(DESTDIR)$(BINPREFIX)/imv
else ifeq ($(WINDOWS),x11)
		$(INSTALL_PROGRAM) $(TARGET_X11) $(DESTDIR)$(BINPREFIX)/imv
else ifeq ($(WINDOWS),all)
		$(INSTALL_PROGRAM) $(TARGET_WAYLAND) $(DESTDIR)$(BINPREFIX)/imv-wayland
		$(INSTALL_PROGRAM) $(TARGET_X11) $(DESTDIR)$(BINPREFIX)/imv-x11
		$(INSTALL_SCRIPT) files/imv $(DESTDIR)$(BINPREFIX)/imv
endif
	$(INSTALL_PROGRAM) $(TARGET_MSG) $(DESTDIR)$(BINPREFIX)/imv-msg
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	$(INSTALL_MAN) doc/imv.1 $(DESTDIR)$(MANPREFIX)/man1/imv.1
	$(INSTALL_MAN) doc/imv-msg.1 $(DESTDIR)$(MANPREFIX)/man1/imv-msg.1
	mkdir -p $(DESTDIR)$(MANPREFIX)/man5
	$(INSTALL_MAN) doc/imv.5 $(DESTDIR)$(MANPREFIX)/man5/imv.5
	mkdir -p $(DESTDIR)$(DATAPREFIX)/applications
	$(INSTALL_DATA) files/imv.desktop $(DESTDIR)$(DATAPREFIX)/applications/imv.desktop
	mkdir -p $(DESTDIR)$(CONFIGPREFIX)
	$(INSTALL_DATA) files/imv_config $(DESTDIR)$(CONFIGPREFIX)/imv_config

uninstall:
ifeq ($(WINDOWS),all)
		$(RM) $(DESTDIR)$(BINPREFIX)/imv-wayland
		$(RM) $(DESTDIR)$(BINPREFIX)/imv-x11
endif
	$(RM) $(DESTDIR)$(BINPREFIX)/imv
	$(RM) $(DESTDIR)$(BINPREFIX)/imv-msg
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/imv.1
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/imv-msg.1
	$(RM) $(DESTDIR)$(MANPREFIX)/man5/imv.5
	$(RM) $(DESTDIR)$(DATAPREFIX)/applications/imv.desktop
	@echo "$(DESTDIR)$(CONFIGPREFIX)/imv_config has not been removed. Please remove it manually."
