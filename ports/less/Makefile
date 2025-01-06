PACKAGE_NAME := less-643
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.gz
TARBALL_FILE := ../../tarballs/$(TARBALL_NAME)

SYSROOT := $(HOME)/argentum/sysroot

all: config
	make -C $(SRC_DIR) 

config: $(SRC_DIR)/Makefile

install: all
	make -C $(SRC_DIR) DESTDIR=$(SYSROOT) install

$(TARBALL_FILE):
	wget "https://www.greenwoodsoftware.com/less/${TARBALL_NAME}" -O $@

$(SRC_DIR): $(TARBALL_FILE)
	tar xf $<

$(SRC_DIR)/Makefile: | $(SRC_DIR)
	cd $(@D) && ./configure --host=$(HOST) \
	                        --sysconfdir=/etc \
	                        --prefix=/usr

clean:
	rm -rf $(SRC_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
