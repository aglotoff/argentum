PACKAGE_NAME := iana-etc-20240125
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.gz
TARBALL_FILE := ../../tarballs/$(TARBALL_NAME)

SYSROOT := $(HOME)/argentum/sysroot

all: install

install: $(SYSROOT)/etc/services $(SYSROOT)/etc/protocols

$(TARBALL_FILE):
	wget "https://github.com/Mic92/iana-etc/releases/download/20240125/${TARBALL_NAME}" -O $@

$(SRC_DIR): $(TARBALL_FILE)
	tar xf $<

$(SRC_DIR)/services: | $(SRC_DIR)
$(SRC_DIR)/protocols: | $(SRC_DIR)

$(SYSROOT)/etc/services: $(SRC_DIR)/services
	mkdir -p $(@D)
	cp $^ $(@D)

$(SYSROOT)/etc/protocols: $(SRC_DIR)/protocols
	mkdir -p $(@D)
	cp $^ $(@D)

clean:
	rm -rf $(SRC_DIR)
