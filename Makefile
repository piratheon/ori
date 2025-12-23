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
	mkdir -p $(SHARE_DIR)/www
	install -D build/ori $(BIN_DIR)/ori
	install -D orpm $(BIN_DIR)/orpm
	cp -r plugins/* $(SHARE_DIR)/
	cp -r www/* $(SHARE_DIR)/www/

uninstall:
	rm -f $(BIN_DIR)/ori
	rm -f $(BIN_DIR)/orpm
	rm -rf $(SHARE_DIR)

clean:
	rm -rf build

package:
	./build.sh
