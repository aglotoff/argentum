#!/bin/bash

PACKAGE_NAME=binutils-2.41
TARBALL_NAME=binutils-2.41.tar.xz
DOWNLOAD_URL="https://ftp.gnu.org/gnu/binutils/${TARBALL_NAME}"
SOURCE_DIR=binutils-2.41
BUILD_DIR=build-binutils
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
                            --disable-nls \
                            --disable-werror &&
  popd &&
  touch ${PACKAGE_NAME}.configured
fi || exit 1

# Build the package
if [ ! -f ${PACKAGE_NAME}.built ]; then
  pushd ${BUILD_DIR} &&
  make &&
  popd &&
  touch ${PACKAGE_NAME}.built
fi || exit 1

# Install the package
if [ ! -f ${PACKAGE_NAME}.installed ]; then
  pushd ${BUILD_DIR} &&
  sudo make install &&
  popd &&
  touch ${PACKAGE_NAME}.installed
fi
