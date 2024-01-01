#!/bin/bash

TARBALL_NAME=gcc-13.2.0.tar.xz
DOWNLOAD_URL="https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/${TARBALL_NAME}"
SOURCE_DIR=gcc-13.2.0
BUILD_DIR=build-gcc
PATCH_FILE=gcc-13.2.0.patch

if [ ! -d ${SOURCE_DIR} ]; then
  if [ ! -f ${TARBALL_NAME} ]; then
    wget ${DOWNLOAD_URL}
  fi

  tar xf ${TARBALL_NAME}
  pushd ${SOURCE_DIR} &&
  patch -p1 -i "../${PATCH_FILE}" &&
  ./contrib/download_prerequisites &&
  popd
fi

if [ ! -d ${BUILD_DIR} ]; then
  mkdir ${BUILD_DIR}
fi

pushd ${BUILD_DIR} &&
../${SOURCE_DIR}/configure --target=arm-none-osdev \
                           --prefix=/usr/local \
                           --with-sysroot=${HOME}/osdev/sysroot \
                           --enable-languages=c,c++ \
                           --disable-nls \
                           --disable-werror \
                           --with-multilib-list=rmprofile \
                           --with-newlib \
                           --disable-decimal-float \
                           --disable-libffi \
                           --disable-libgomp \
                           --disable-libmudflap \
                           --disable-libquadmath  \
                           --disable-libssp \
                           --disable-libstdcxx-pch \
                           --disable-shared \
                           --disable-threads \
                           --disable-tls  

make all-gcc all-target-libgcc &&
sudo make install-gcc install-target-libgcc &&
popd
