#!/bin/bash

set -e

SCRIPT_LOCATION=$(cd "$(dirname "$0")"; pwd)
. ${SCRIPT_LOCATION}/common.sh

[ -d ${SOURCE_DIR}/ci ] || die "CWD must be repository root"
[ -d $BUILD_DIR       ] || die "build location '${BUILD_DIR}' doesn't exist"

cd $BUILD_DIR
make install
[ -d $INSTALL_DIR     ] || die "install location '${INSTALL_DIR}' doesn't exist"

cd $INSTALL_DIR
tar czvf cpprest.tar.gz *
