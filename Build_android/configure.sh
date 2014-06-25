#!/bin/bash
set -e

# Note: we require android ndk r9d available from
# https://dl.google.com/android/ndk/android-ndk-r9d-linux-x86_64.tar.bz2
# https://dl.google.com/android/ndk/android-ndk-r9d-windows-x86_64.zip

# Variables setup

if [ ! -e "${ANDROID_NDK}" ]
then
    echo "ANDROID_NDK does not point to a valid NDK."
    echo "(current setting: '${ANDROID_NDK}')"
    exit
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

# -------
# Openssl
# -------

# This steps are based on the github project openssl1.0.1g-android
# https://github.com/aluvalasuman/openssl1.0.1g-android
(
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
    ./Configure android no-shared --prefix="${SRC_DIR}/openssl/r9d-9-armeabiv7" --openssldir=openssl
    make all install_sw
)

# --------
# libiconv
# --------

# This steps are based on the blog post
# http://danilogiulianelli.blogspot.com/2012/12/how-to-cross-compile-libiconv-for.html

(
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
    "${NDK_DIR}/ndk-build"
    cd ..
    mkdir -p r9d-9-armeabiv7a/include
    mkdir -p r9d-9-armeabiv7a/lib
    cp libiconv-1.13.1/include/iconv.h r9d-9-armeabiv7a/include/
    cp libs/armeabi-v7a/libiconv.so r9d-9-armeabiv7a/lib/
)

# -----
# Boost
# -----
# Uses the script from MysticTreeGames

(
    git clone https://github.com/MysticTreeGames/Boost-for-Android.git
    cd Boost-for-Android
    git checkout 8075d96cc9ef42d5c52d233b8ee42cb8421a2818
    patch -p1 < "$DIR/boost-for-android.patch"
    ./build-android.sh "${NDK_DIR}"
)

# -------------
# android-cmake
# -------------

git clone https://github.com/taka-no-me/android-cmake.git

# ----------
# casablanca
# ----------

(
    mkdir -p build.armv7.debug
    cd build.armv7.debug
    cmake "$DIR/../Release/" \
	-DCMAKE_TOOLCHAIN_FILE=../android-cmake/android.toolchain.cmake \
	-DANDROID_ABI=armeabi-v7a \
	-DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-4.8 \
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
	-DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-4.8 \
	-DANDROID_STL=none \
	-DANDROID_STL_FORCE_FEATURES=ON \
	-DANDROID_NDK="${ANDROID_NDK}" \
	-DANDROID_NATIVE_API_LEVEL=android-9 \
	-DANDROID_GOLD_LINKER=OFF \
	-DCMAKE_BUILD_TYPE=Release
    make -j 3
)
