CC = gcc
PKG = gtk+-3.0 vte-2.91
CFLAGS = $(shell pkg-config --cflags $(PKG)) -O2 -Wall
LIBS = $(shell pkg-config --libs $(PKG))

TARGET = wyrmshell
SRC = main.c

PREFIX ?= $(HOME)/.local

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

install: $(TARGET)
	mkdir -p $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	mkdir -p $(PREFIX)/share/applications
	sed "s|@BINPATH@|$(PREFIX)/bin/$(TARGET)|" wyrmshell.desktop.in > $(PREFIX)/share/applications/wyrmshell.desktop
	update-desktop-database $(PREFIX)/share/applications 2>/dev/null || true
	mkdir -p $(HOME)/.config/fastfetch
	cp -n fastfetch/config.jsonc $(HOME)/.config/fastfetch/config.jsonc || true
	cp -n fastfetch/dragon-logo.txt $(HOME)/.config/fastfetch/dragon-logo.txt || true
	mkdir -p $(HOME)/.config/wyrmshell
	cp -n wyrmshell-config/dragon-ansi.txt $(HOME)/.config/wyrmshell/dragon-ansi.txt || true

clean:
	rm -f $(TARGET)

.PHONY: all install clean
