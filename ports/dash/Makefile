PACKAGE_NAME := dash-0.5.12
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.gz
TARBALL_FILE := ../../tarballs/$(TARBALL_NAME)

SYSROOT := $(HOME)/argentum/sysroot

all: config
	make -C $(SRC_DIR) 

config: $(SRC_DIR)/Makefile

install: all
	make -C $(SRC_DIR) DESTDIR=$(SYSROOT) install
	mkdir -p $(SYSROOT)/bin
	ln -f $(SYSROOT)/usr/bin/dash $(SYSROOT)/bin/sh

$(TARBALL_FILE):
	wget "http://gondor.apana.org.au/~herbert/dash/files/${TARBALL_NAME}" -O $@

$(SRC_DIR): $(TARBALL_FILE)
	tar xf $<

$(SRC_DIR)/Makefile: | $(SRC_DIR)
	cd $(@D) && ./configure --host=$(HOST) \
	                        --prefix=/usr

clean:
	rm -rf $(SRC_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
