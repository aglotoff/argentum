#!/bin/bash

TARBALL_NAME=gzip-1.12.tar.xz
DOWNLOAD_URL="https://ftp.gnu.org/gnu/gzip/${TARBALL_NAME}"
BUILD_DIR=gzip-1.12
PATCH_FILE=gzip-1.12.patch

if [ ! -d ${BUILD_DIR} ]; then
  if [ ! -f ${TARBALL_NAME} ]; then
    wget ${DOWNLOAD_URL}
  fi || exit 1

  tar xf ${TARBALL_NAME}
  pushd ${BUILD_DIR} &&
  patch -p1 -i "../${PATCH_FILE}" &&
  popd
fi || exit 1

export CFLAGS="-O2 -mcpu=cortex-a9 -mhard-float"

if [ ! -f ${BUILD_DIR}/Makefile ]; then
  pushd ${BUILD_DIR} &&
  ./configure --host=arm-none-osdev \
              --prefix=/usr &&
  popd
fi || exit 1

pushd ${BUILD_DIR} &&
make &&
make DESTDIR=${HOME}/osdev/sysroot install &&
popd || exit 1
