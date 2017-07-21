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

if is_osx; then
    cpprest_lib_name="$(find . -name "libcpprest*" -type file -exec basename {} \;)"
    cpprest_lib_path="${INSTALL_PREFIX}/lib/${cpprest_lib_name}"
    cpprest_lib_id="/${cpprest_lib_path}"
    echo "setting rpath of ${cpprest_lib_path} to ${cpprest_lib_id}"
    install_name_tool -id $cpprest_lib_id $cpprest_lib_path
fi

tar czvf cpprest.tar.gz *
