# Makefile for ORI Terminal Assistant

# Variables
PREFIX?=/usr/local
DESTDIR?=
BIN_DIR=$(DESTDIR)$(PREFIX)/bin
SHARE_DIR=$(DESTDIR)$(PREFIX)/share/ori

# Phony targets
.PHONY: all install uninstall clean package

all: build/ori

build/ori:
	mkdir -p build
	cd build && cmake .. -DCMAKE_INSTALL_PREFIX=$(PREFIX) && make

install: all
	mkdir -p $(BIN_DIR)
	mkdir -p $(SHARE_DIR)
	install -D build/ori $(BIN_DIR)/ori

uninstall:
	rm -f $(BIN_DIR)/ori
	rm -rf $(SHARE_DIR)

clean:
	rm -rf build

package:
	./build.sh
