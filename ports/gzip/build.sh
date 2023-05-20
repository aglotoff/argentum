#!/bin/bash

TARBALL_NAME=gzip-1.12.tar.xz
DOWNLOAD_URL="https://ftp.gnu.org/gnu/gzip/${TARBALL_NAME}"
BUILD_DIR=gzip-1.12
PATCH_FILE=gzip-1.12.patch

if [ ! -d ${BUILD_DIR} ]; then
  if [ ! -f ${TARBALL_NAME} ]; then
    wget ${DOWNLOAD_URL}
  fi

  tar xf ${TARBALL_NAME}
  pushd ${BUILD_DIR} &&
  patch -p1 -i "../${PATCH_FILE}" &&
  popd
fi

pushd ${BUILD_DIR} &&
./configure --host=arm-none-argentum --prefix=/usr &&
make &&
make DESTDIR="../../../sysroot" install &&
popd
