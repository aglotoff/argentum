#!/bin/bash

TARBALL_NAME=binutils-2.41.tar.xz
DOWNLOAD_URL="https://ftp.gnu.org/gnu/binutils/${TARBALL_NAME}"
SOURCE_DIR=binutils-2.41
BUILD_DIR=build-binutils
PATCH_FILE=binutils-2.41.patch

if [ ! -d ${SOURCE_DIR} ]; then
  if [ ! -f ${TARBALL_NAME} ]; then
    wget ${DOWNLOAD_URL}
  fi

  tar xf ${TARBALL_NAME}
  pushd ${SOURCE_DIR} &&
  patch -p1 -i "../${PATCH_FILE}" &&
  popd
fi

if [ ! -d ${BUILD_DIR} ]; then
  mkdir ${BUILD_DIR}
fi

pushd ${BUILD_DIR} &&
../${SOURCE_DIR}/configure --target=arm-none-osdev \
                           --prefix=/usr/local \
                           --with-sysroot=${HOME}/osdev/sysroot \
                           --disable-nls \
                           --disable-werror
make &&
sudo make install &&
popd
