PACKAGE_NAME := sed-4.9
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.xz
PATCH_FILE   := $(PACKAGE_NAME).patch

SYSROOT := $(HOME)/osdev/sysroot

all: config
	make -C $(SRC_DIR)

config: $(SRC_DIR)/Makefile

install: all
	make -C $(SRC_DIR) DESTDIR=$(SYSROOT) install
	ln -f $(SYSROOT)/usr/bin/dash $(SYSROOT)/bin/sh

$(TARBALL_NAME):
	wget "https://ftp.gnu.org/gnu/sed/${TARBALL_NAME}"

$(SRC_DIR): $(TARBALL_NAME)
	tar xf $<
	cd $@ && patch -p1 -i ../$(PATCH_FILE)

$(SRC_DIR)/Makefile: | $(SRC_DIR)
	cd $(@D) && \
	 ./configure --host=arm-none-osdev \
	                                 --prefix=/usr

clean:
	rm -rf $(SRC_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
