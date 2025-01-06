PACKAGE_NAME := gzip-1.12
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.xz
TARBALL_FILE := ../../tarballs/$(TARBALL_NAME)
PATCH_FILE   := $(PACKAGE_NAME).patch

SYSROOT := $(HOME)/argentum/sysroot

all: config
	make -C $(SRC_DIR) 
config: $(SRC_DIR)/Makefile

install: all
	make -C $(SRC_DIR) DESTDIR=$(SYSROOT) install

$(TARBALL_FILE):
	wget "https://ftp.gnu.org/gnu/gzip/${TARBALL_NAME}" -O $@

$(SRC_DIR): $(TARBALL_FILE)
	tar xf $<
	cd $@ && patch -p1 -i ../$(PATCH_FILE)

$(SRC_DIR)/Makefile: | $(SRC_DIR)
	cd $(@D) && ./configure --host=$(HOST) \
	                        --prefix=/usr

clean:
	rm -rf $(SRC_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
