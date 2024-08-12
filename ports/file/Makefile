PACKAGE_NAME := file-5.45
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.gz
TARBALL_FILE := ../../tarballs/$(TARBALL_NAME)
PATCH_FILE   := $(PACKAGE_NAME).patch

SYSROOT := $(HOME)/argentum/sysroot

all: $(SRC_DIR)/build/src/file config
	make -C $(SRC_DIR) FILE_COMPILE=$(shell pwd)/$(SRC_DIR)/build/src/file

config: $(SRC_DIR)/Makefile

$(SRC_DIR)/build/src/file: | $(SRC_DIR)
	mkdir -p $(SRC_DIR)/build && \
	cd $(SRC_DIR)/build && \
		../configure --disable-bzlib      \
								--disable-libseccomp \
								--disable-xzlib      \
								--disable-zlib && \
		make

install: all
	make -C $(SRC_DIR) DESTDIR=$(SYSROOT) install
	mkdir -p $(SYSROOT)/etc

$(TARBALL_FILE):
	wget "https://astron.com/pub/file/${TARBALL_NAME}" -O $@

$(SRC_DIR): $(TARBALL_FILE)
	tar xf $<
	cd $@ && patch -p1 -i ../$(PATCH_FILE)

$(SRC_DIR)/Makefile: | $(SRC_DIR)
	cd $(@D) && ./configure --host=arm-none-argentum \
	                        --build=$(shell $(SRC_DIR)/build-aux/config.guess) \
	                        --prefix=/usr

clean:
	rm -rf $(SRC_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
