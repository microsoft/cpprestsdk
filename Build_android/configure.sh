#!/bin/bash
# ==++==
#
# Copyright (c) Microsoft Corporation. All rights reserved. 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# ==--==
# =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
#
# configure.sh
#
# Build script for casablanca on android
#
# For the latest on this and related APIs, please see http://casablanca.codeplex.com.
#
# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

set -e

# Note: we require android ndk r10 available from
# http://dl.google.com/android/ndk/android-ndk32-r10-linux-x86_64.tar.bz2
# http://dl.google.com/android/ndk/android-ndk32-r10-windows-x86_64.zip

# -----------------
# Parse args
# -----------------

DO_LIBICONV=1
DO_BOOST=1
DO_OPENSSL=1

function usage {
    echo "Usage: $0 [--skip-boost] [--skip-openssl] [--skip-libiconv] [-h] [--ndk <android-ndk>]"
    echo ""
    echo "    --skip-boost          Skip fetching and compiling boost"
    echo "    --skip-openssl        Skip fetching and compiling openssl"
    echo "    --skip-libiconv       Skip fetching and compiling libiconv"
    echo "    -h,--help,-?          Display this information"
    echo "    --ndk <android-ndk>   If specified, overrides the ANDROID_NDK environment variable"
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
	"--skip-libiconv")
	    DO_LIBICONV=0
	    ;;
	"-?"|"-h"|"--help")
	    usage
	    exit
	    ;;
	"--ndk")
	    shift
	    export ANDROID_NDK=$1
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

# This steps are based on the github project openssl1.0.1g-android
# https://github.com/aluvalasuman/openssl1.0.1g-android
if [ "${DO_OPENSSL}" == "1" ]
then
(
    rm -rf openssl
    mkdir openssl
    cd openssl
    if [ ! -e "openssl-1.0.1h.tar.gz" ]
    then
	wget http://www.openssl.org/source/openssl-1.0.1h.tar.gz
    fi
    rm -rf openssl-1.0.1h
    tar xzf openssl-1.0.1h.tar.gz
    cd openssl-1.0.1h
    export ANDROID_NDK="$NDK_DIR"
    . "${DIR}/android_configure_armeabiv7.sh"
    ./Configure android no-shared --openssldir="${SRC_DIR}/openssl/r9d-9-armeabiv7"
    make all install_sw || exit 1
)
fi

# --------
# libiconv
# --------

# This steps are based on the blog post
# http://danilogiulianelli.blogspot.com/2012/12/how-to-cross-compile-libiconv-for.html
if [ "${DO_LIBICONV}" == "1" ]
then
(
    rm -rf libiconv
    mkdir libiconv
    cd libiconv
    if [ ! -e "libiconv-1.13.1.tar.gz" ]
    then
	wget http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.13.1.tar.gz
    fi
    rm -rf libiconv-1.13.1
    tar xzf libiconv-1.13.1.tar.gz
    patch -b -p0 < "$DIR/libiconv/libiconv.patch"
    cd libiconv-1.13.1
    ./configure
    cp -r "$DIR/libiconv/jni" ..
    cd ../jni
    "${NDK_DIR}/ndk-build" || exit 1
    cd ..
    mkdir -p r9d-9-armeabiv7a/include
    mkdir -p r9d-9-armeabiv7a/lib
    cp libiconv-1.13.1/include/iconv.h r9d-9-armeabiv7a/include/
    cp libs/armeabi-v7a/libiconv.so r9d-9-armeabiv7a/lib/
)
fi

# -----
# Boost
# -----
# Uses the script from MysticTreeGames

if [ "${DO_BOOST}" == "1" ]
then
(
    rm -rf Boost-for-Android
    git clone https://github.com/MysticTreeGames/Boost-for-Android.git
    cd Boost-for-Android
    git checkout 1c95d349d5f92c5ac1c24e0ec6985272a3e3883c
    patch -p1 < "$DIR/boost-for-android.patch"
    PATH="$PATH:$NDK_DIR" ./build-android.sh --boost=1.55.0 --with-libraries=locale,random,date_time,filesystem,system,thread,chrono "${NDK_DIR}" || exit 1
)
fi

# -------------
# android-cmake
# -------------
if [ ! -e android-cmake ]
then
    git clone https://github.com/taka-no-me/android-cmake.git
fi

# ----------
# casablanca
# ----------

(
    mkdir -p build.armv7.debug
    cd build.armv7.debug
    cmake "$DIR/../Release/" \
	-DCMAKE_TOOLCHAIN_FILE=../android-cmake/android.toolchain.cmake \
	-DANDROID_ABI=armeabi-v7a \
	-DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-clang3.4 \
	-DANDROID_STL=none \
	-DANDROID_STL_FORCE_FEATURES=ON \
        -DANDROID_NATIVE_API_LEVEL=android-9 \
	-DANDROID_GOLD_LINKER=OFF \
	-DCMAKE_BUILD_TYPE=Debug \
	-DANDROID_NDK="${ANDROID_NDK}"
    make -j 3
)

(
    mkdir -p build.armv7.release
    cd build.armv7.release
    cmake "$DIR/../Release/" \
	-DCMAKE_TOOLCHAIN_FILE=../android-cmake/android.toolchain.cmake \
	-DANDROID_ABI=armeabi-v7a \
	-DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-clang3.4 \
	-DANDROID_STL=none \
	-DANDROID_STL_FORCE_FEATURES=ON \
	-DANDROID_NDK="${ANDROID_NDK}" \
	-DANDROID_NATIVE_API_LEVEL=android-9 \
	-DANDROID_GOLD_LINKER=OFF \
	-DCMAKE_BUILD_TYPE=Release
    make -j 3
)
