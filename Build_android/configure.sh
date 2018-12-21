#!/bin/bash
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
# =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
#
# configure.sh
#
# Build script for casablanca on android
#
# For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
#
# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

set -e

# The Android NDK r10e or later may work, but we test with r18b. To download, see the following link:
# https://developer.android.com/ndk/downloads/index.html

# -----------------
# Parse args
# -----------------

DO_BOOST=1
DO_OPENSSL=1
DO_CPPRESTSDK=1

BOOSTVER=1.65.1
OPENSSLVER=1.0.2k

API=15
STL=c++_static

function usage {
    echo "Usage: $0 [--skip-boost] [--skip-openssl] [--skip-cpprestsdk] [-h] [--ndk <android-ndk>]"
    echo ""
    echo "    --skip-boost          Skip fetching and compiling boost"
    echo "    --skip-openssl        Skip fetching and compiling openssl"
    echo "    --skip-cpprestsdk     Skip compiling cpprestsdk"
    echo "    --boost <version>     Override the Boost version to build (default is ${BOOSTVER})"
    echo "    --openssl <version>   Override the OpenSSL version to build (default is ${OPENSSLVER})"
    echo "    --ndk <android-ndk>   If specified, overrides the ANDROID_NDK environment variable"
    echo "    -h,--help,-?          Display this information"
}

while [[ $# > 0 ]]
do
    case $1 in
    "--skip-boost")
        DO_BOOST=0
        ;;
    "--skip-openssl")
        DO_OPENSSL=0
        ;;
    "--skip-cpprestsdk")
        DO_CPPRESTSDK=0
        ;;
    "--boost")
        shift
        DO_BOOST=1
        BOOSTVER=$1
        ;;
    "--openssl")
        shift
        DO_OPENSSL=1
        OPENSSLVER=$1
        ;;
    "--ndk")
        shift
        export ANDROID_NDK=$1
        ;;
    "-?"|"-h"|"--help")
        usage
        exit
        ;;
    *)
        usage
        exit 1
        ;;
    esac
    shift
done

# Variables setup

if [ ! -e "${ANDROID_NDK}/ndk-build" ]
then
    echo "ANDROID_NDK does not point to a valid NDK."
    echo "(current setting: '${ANDROID_NDK}')"
    exit 1
fi
NDK_DIR=`cd "${ANDROID_NDK}" && pwd`
SRC_DIR=`pwd`

if [ -z "$NCPU" ]; then
	NCPU=4
	if uname -s | grep -i "linux" > /dev/null ; then
		NCPU=`cat /proc/cpuinfo | grep -c -i processor`
	fi
fi

# -----------------------
# Identify the script dir
# -----------------------

# source:
# http://stackoverflow.com/questions/59895/can-a-bash-script-tell-what-directory-its-stored-in

SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

if [ "$SRC_DIR" == "$DIR" ]
then
    echo "You must use a separate directory for building."
    echo "(try 'mkdir build; cd build; ../configure.sh')"
    exit 1
fi

# -------
# Openssl
# -------

# This steps are based on the official openssl build instructions
# http://wiki.openssl.org/index.php/Android
if [ "${DO_OPENSSL}" == "1" ]; then (
    if [ ! -d "openssl" ]; then mkdir openssl; fi
    cd openssl
    cp -af "${DIR}/openssl/." .
    make all ANDROID_NDK="${NDK_DIR}" ANDROID_TOOLCHAIN=clang ANDROID_GCC_VERSION=4.9 ANDROID_ABI=armeabi-v7a OPENSSL_PREFIX=armeabi-v7a OPENSSL_VERSION=$OPENSSLVER
    make all ANDROID_NDK="${NDK_DIR}" ANDROID_TOOLCHAIN=clang ANDROID_GCC_VERSION=4.9 ANDROID_ABI=x86 OPENSSL_PREFIX=x86 OPENSSL_VERSION=$OPENSSLVER
) fi

# -----
# Boost
# -----
# Uses the build script from Moritz Wundke (formerly MysticTreeGames)
# https://github.com/moritz-wundke/Boost-for-Android

if [ "${DO_BOOST}" == "1" ]; then (
    if [ ! -d 'Boost-for-Android' ]; then git clone https://github.com/moritz-wundke/Boost-for-Android; fi
    cd Boost-for-Android
    git checkout 84973078a3d7668067d422d4654696ef59ab9d6d
    PATH="$PATH:$NDK_DIR" \
    CXXFLAGS="-std=gnu++11" \
    ./build-android.sh \
        --boost=$BOOSTVER \
        --arch=armeabi-v7a,x86 \
        --with-libraries=atomic,random,date_time,filesystem,system,thread,chrono \
        "${NDK_DIR}" || exit 1
) fi

# ----------
# casablanca
# ----------

if [ "${DO_CPPRESTSDK}" == "1" ]; then
    # Use the builtin CMake toolchain configuration that comes with the NDK
    function build_cpprestsdk { (
        mkdir -p $1
        cd $1
        cmake "${DIR}/.." \
            -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK}/build/cmake/android.toolchain.cmake" \
            -DANDROID_NDK="${ANDROID_NDK}" \
            -DANDROID_TOOLCHAIN=clang \
            -DANDROID_ABI=$2 \
            -DBOOST_VERSION="${BOOSTVER}" \
            -DCMAKE_BUILD_TYPE=$3
        make -j $NCPU
    ) }

    # Build the cpprestsdk for each target configuration
    build_cpprestsdk build.armv7.debug armeabi-v7a Debug
    build_cpprestsdk build.armv7.release armeabi-v7a Release
    build_cpprestsdk build.x86.debug x86 Debug
    build_cpprestsdk build.x86.release x86 Release
fi
