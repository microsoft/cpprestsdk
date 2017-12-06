#!/bin/bash
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
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

DO_BOOST=1
DO_OPENSSL=1
DO_CPPRESTSDK=1

function usage {
    echo "Usage: $0 [--skip-boost] [--skip-openssl] [--skip-cpprestsdk] [-h] [--ndk <android-ndk>]"
    echo ""
    echo "    --skip-boost          Skip fetching and compiling boost"
    echo "    --skip-openssl        Skip fetching and compiling openssl"
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

	PATH="$PATH:$NDK_DIR" ./build-android.sh --boost=1.55.0 --with-libraries=random,date_time,filesystem,system,thread,chrono "${NDK_DIR}" || exit 1
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

	PATH="$PATH:$NDK_DIR" ./build-android.sh --boost=1.55.0 --with-libraries=atomic,random,date_time,filesystem,system,thread,chrono "${NDK_DIR}" || exit 1
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
	    -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-clang3.8 \
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
	    -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-clang3.8 \
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
	    -DANDROID_TOOLCHAIN_NAME=x86-clang3.8 \
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
	    -DANDROID_TOOLCHAIN_NAME=x86-clang3.8 \
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
