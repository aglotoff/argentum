PACKAGE_NAME := dash-0.5.12
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.gz

SYSROOT := $(HOME)/osdev/sysroot

all: config
	make -C $(SRC_DIR) 

config: $(SRC_DIR)/Makefile

install: all
	make -C $(SRC_DIR) DESTDIR=$(SYSROOT) install
	ln -f $(SYSROOT)/usr/bin/dash $(SYSROOT)/bin/sh

$(TARBALL_NAME):
	wget "http://gondor.apana.org.au/~herbert/dash/files/${TARBALL_NAME}"

$(SRC_DIR): $(TARBALL_NAME)
	tar xf $<

$(SRC_DIR)/Makefile: | $(SRC_DIR)
	cd $(@D) && ./configure --host=arm-none-osdev \
	                        --prefix=/usr

clean:
	rm -rf $(SRC_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
