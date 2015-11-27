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
# For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
#
# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

set -e

# Note: we require android ndk r10e available from
# http://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.tar.bz2
# http://dl.google.com/android/ndk/android-ndk-r10e-windows-x86_64.zip

# -----------------
# Parse args
# -----------------

DO_LIBICONV=1
DO_BOOST=1
DO_OPENSSL=1
DO_CPPRESTSDK=1

function usage {
    echo "Usage: $0 [--skip-boost] [--skip-openssl] [--skip-libiconv] [--skip-cpprestsdk] [-h] [--ndk <android-ndk>]"
    echo ""
    echo "    --skip-boost          Skip fetching and compiling boost"
    echo "    --skip-openssl        Skip fetching and compiling openssl"
    echo "    --skip-libiconv       Skip fetching and compiling libiconv"
    echo "    --skip-cpprestsdk     Skip compiling cpprestsdk"
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
	"--skip-cpprestsdk")
	    DO_CPPRESTSDK=0
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

# This steps are based on the official openssl build instructions
# http://wiki.openssl.org/index.php/Android
if [ "${DO_OPENSSL}" == "1" ]
then
(
    if [ ! -d "openssl" ]; then mkdir openssl; fi
    cd openssl
    cp "${DIR}/openssl/Makefile" .
    export ANDROID_NDK_ROOT="${NDK_DIR}"
    make all
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
    if [ ! -e "libiconv-1.13.1.tar.gz" ]
    then
	wget http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.13.1.tar.gz
    fi
    rm -rf libiconv
    mkdir libiconv
    cd libiconv
    tar xzf ../libiconv-1.13.1.tar.gz
    patch -b -p0 < "$DIR/libiconv/libiconv.patch"
    cd libiconv-1.13.1
    ./configure
    cp -r "$DIR/libiconv/jni" ..
    cd ../jni
    "${NDK_DIR}/ndk-build" || exit 1
    cd ..
    mkdir -p armeabi-v7a/include
    mkdir -p armeabi-v7a/lib
    mkdir -p x86/include
    mkdir -p x86/lib
    cp libiconv-1.13.1/include/iconv.h armeabi-v7a/include/
    cp libiconv-1.13.1/include/iconv.h x86/include/
    cp obj/local/x86/libiconv.a x86/lib/
    cp obj/local/armeabi-v7a/libiconv.a armeabi-v7a/lib/
)
fi

# -----
# Boost
# -----
# Uses the script from MysticTreeGames

if [ "${DO_BOOST}" == "1" ]
then
(
    (
	if [ ! -d "Boost-for-Android" ]
	then
	    git clone https://github.com/MysticTreeGames/Boost-for-Android.git
	fi
	cd Boost-for-Android
	if [ ! -e "cpprestsdk.patched.stamp" ]
	then
	    git checkout 1c95d349d5f92c5ac1c24e0ec6985272a3e3883c
	    git reset --hard HEAD
	    git apply "$DIR/boost-for-android.patch"
	    touch cpprestsdk.patched.stamp
	fi
	PATH="$PATH:$NDK_DIR" ./build-android.sh --boost=1.55.0 --with-libraries=locale,random,date_time,filesystem,system,thread,chrono "${NDK_DIR}" || exit 1
    )

    (
	if [ ! -d "Boost-for-Android-x86" ]
	then
	    git clone Boost-for-Android Boost-for-Android-x86
	fi
	cd Boost-for-Android-x86
	if [ ! -e "cpprestsdk.patched.stamp" ]
	then
	    git checkout 1c95d349d5f92c5ac1c24e0ec6985272a3e3883c
	    git reset --hard HEAD
	    git apply "$DIR/boost-for-android-x86.patch"
	    ln -s ../Boost-for-Android/boost_1_55_0.tar.bz2 .
	    touch cpprestsdk.patched.stamp
	fi
	PATH="$PATH:$NDK_DIR" ./build-android.sh --boost=1.55.0 --with-libraries=locale,random,date_time,filesystem,system,thread,chrono "${NDK_DIR}" || exit 1
    )
)
fi

if [ "${DO_CPPRESTSDK}" == "1" ]
then
(
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
	    -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-clang3.6 \
	    -DANDROID_STL=none \
	    -DANDROID_STL_FORCE_FEATURES=ON \
            -DANDROID_NATIVE_API_LEVEL=android-9 \
	    -DANDROID_GOLD_LINKER=OFF \
	    -DCMAKE_BUILD_TYPE=Debug \
	    -DANDROID_NDK="${ANDROID_NDK}"
	make -j 1
    )

    (
	mkdir -p build.armv7.release
	cd build.armv7.release
	cmake "$DIR/../Release/" \
	    -DCMAKE_TOOLCHAIN_FILE=../android-cmake/android.toolchain.cmake \
	    -DANDROID_ABI=armeabi-v7a \
	    -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-clang3.6 \
	    -DANDROID_STL=none \
	    -DANDROID_STL_FORCE_FEATURES=ON \
	    -DANDROID_NDK="${ANDROID_NDK}" \
	    -DANDROID_NATIVE_API_LEVEL=android-9 \
	    -DANDROID_GOLD_LINKER=OFF \
	    -DCMAKE_BUILD_TYPE=Release
	make -j 1
    )

    (
	mkdir -p build.x86.debug
	cd build.x86.debug
	cmake "$DIR/../Release/" \
	    -DCMAKE_TOOLCHAIN_FILE=../android-cmake/android.toolchain.cmake \
	    -DANDROID_ABI=x86 \
	    -DANDROID_TOOLCHAIN_NAME=x86-clang3.6 \
	    -DANDROID_STL=none \
	    -DANDROID_STL_FORCE_FEATURES=ON \
            -DANDROID_NATIVE_API_LEVEL=android-9 \
	    -DANDROID_GOLD_LINKER=OFF \
	    -DCMAKE_BUILD_TYPE=Debug \
	    -DANDROID_NDK="${ANDROID_NDK}"
	make -j 1
    )

    (
	mkdir -p build.x86.release
	cd build.x86.release
	cmake "$DIR/../Release/" \
	    -DCMAKE_TOOLCHAIN_FILE=../android-cmake/android.toolchain.cmake \
	    -DANDROID_ABI=x86 \
	    -DANDROID_TOOLCHAIN_NAME=x86-clang3.6 \
	    -DANDROID_STL=none \
	    -DANDROID_STL_FORCE_FEATURES=ON \
	    -DANDROID_NDK="${ANDROID_NDK}" \
	    -DANDROID_NATIVE_API_LEVEL=android-9 \
	    -DANDROID_GOLD_LINKER=OFF \
	    -DCMAKE_BUILD_TYPE=Release
	make -j 1
    )
)
fi