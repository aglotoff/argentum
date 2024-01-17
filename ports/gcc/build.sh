#!/bin/bash

PACKAGE_NAME=gcc-13.2.0
TARBALL_NAME=gcc-13.2.0.tar.xz
DOWNLOAD_URL="https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/${TARBALL_NAME}"
SOURCE_DIR=gcc-13.2.0
BUILD_DIR=build-gcc
PATCH_FILE=${PACKAGE_NAME}.patch

# Download and patch the package contents
if [ ! -f ${PACKAGE_NAME}.unpacked ]; then
  if [ ! -f ${TARBALL_NAME} ]; then
    wget ${DOWNLOAD_URL}
  fi || exit 1

  rm -rf ${SOURCE_DIR}

  tar xf ${TARBALL_NAME} &&
  pushd ${SOURCE_DIR} &&
  patch -p1 -i "../${PATCH_FILE}" &&
  ./contrib/download_prerequisites &&
  popd &&
  touch ${PACKAGE_NAME}.unpacked
fi || exit 1

# Create the build directory
if [ ! -d ${BUILD_DIR} ]; then
  mkdir ${BUILD_DIR}
fi || exit 1

# Configure the package
if [ ! -f ${PACKAGE_NAME}.configured ]; then
  pushd ${BUILD_DIR} &&
  ../${SOURCE_DIR}/configure --target=arm-none-osdev \
                             --prefix=/usr/local \
                             --with-sysroot=${HOME}/osdev/sysroot \
                             --without-headers \
                             --enable-languages=c,c++ \
                             --with-cpu=cortex-a9 \
                             --with-float=hard \
                             --with-fpu=vfp \
                             --with-newlib \
                             --disable-nls \
                             --disable-werror \
                             --disable-multilib \
                             --disable-decimal-float \
                             --disable-libffi \
                             --disable-libgomp \
                             --disable-libmudflap \
                             --disable-libquadmath  \
                             --disable-libssp \
                             --disable-libstdcxx-pch \
                             --disable-shared \
                             --disable-threads \
                             --disable-tls &&
  popd &&
  touch ${PACKAGE_NAME}.configured
fi || exit 1

# Build the package
if [ ! -f ${PACKAGE_NAME}.built ]; then
  pushd ${BUILD_DIR} &&
  make all-gcc all-target-libgcc &&
  popd &&
  touch ${PACKAGE_NAME}.built
fi || exit 1

# Install the package
if [ ! -f ${PACKAGE_NAME}.installed ]; then
  pushd ${BUILD_DIR} &&
  sudo make install-gcc install-target-libgcc &&
  popd &&
  touch ${PACKAGE_NAME}.installed
fi

# Build the package
if [ ! -f ${PACKAGE_NAME}.libstdc++-v3.built ]; then
  pushd ${BUILD_DIR} &&
  make all-target-libstdc++-v3 &&
  popd &&
  touch ${PACKAGE_NAME}.libstdc++-v3.built
fi || exit 1

# Install the package
if [ ! -f ${PACKAGE_NAME}.libstdc++-v3.installed ]; then
  pushd ${BUILD_DIR} &&
  sudo make install-target-libstdc++-v3 &&
  popd &&
  touch ${PACKAGE_NAME}.libstdc++-v3.installed
fi
