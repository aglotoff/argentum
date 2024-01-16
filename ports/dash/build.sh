#!/bin/bash

TARBALL_NAME=dash-0.5.12.tar.gz
DOWNLOAD_URL="http://gondor.apana.org.au/~herbert/dash/files/${TARBALL_NAME}"
BUILD_DIR=dash-0.5.12

if [ ! -d ${BUILD_DIR} ]; then
  if [ ! -f ${TARBALL_NAME} ]; then
    wget ${DOWNLOAD_URL}
  fi || exit 1

  tar xf ${TARBALL_NAME}
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
