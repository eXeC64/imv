.PHONY: imv obj clean check install uninstall

PREFIX ?= /usr
BINPREFIX ?= $(PREFIX)/bin
MANPREFIX ?= $(PREFIX)/share/man
DATAPREFIX ?= $(PREFIX)/share

ifeq ($(V),)
MUTE :=	@
endif

CFLAGS ?= -W -Wall -Wpedantic
CFLAGS += -std=gnu11 $(shell sdl2-config --cflags)
LDFLAGS += $(shell sdl2-config --libs) -lfreeimage -lSDL2_ttf -lfontconfig -lpthread

TARGET = $(BUILDDIR)/imv
BUILDDIR ?= build

SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))
TESTS = $(patsubst test/%.c,$(BUILDDIR)/test_%,$(wildcard test/*.c))

VERSION = "v1.2.0"

CFLAGS += -DIMV_VERSION=\"$(VERSION)\"

imv: $(TARGET)

$(TARGET): obj
	@echo "LINKING $@"
	$(MUTE)$(CC) -o $@ $(OBJECTS) $(LDLIBS) $(LDFLAGS)

debug: CFLAGS += -DDEBUG -g -pg
debug: $(TARGET)

obj: $(BUILDDIR) $(OBJECTS)

$(BUILDDIR):
	$(MUTE)mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: src/%.c
	@echo "COMPILING $@"
	$(MUTE)$(CC) -c $(CFLAGS) -o $@ $<

$(BUILDDIR)/test_%: test/%.c src/%.c
	@echo "BUILDING $@"
	$(MUTE)$(CC) -o $@ -Isrc -W -Wall $(CFLAGS) $(LDFLAGS) -std=gnu11 -lcmocka $^

check: $(BUILDDIR) $(TESTS)
	@echo "RUNNING TESTS"
	$(MUTE)for t in "$(TESTS)"; do $$t; done

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
