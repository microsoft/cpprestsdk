# Note: we require android ndk r9d available from
# https://dl.google.com/android/ndk/android-ndk-r9d-linux-x86_64.tar.bz2
# https://dl.google.com/android/ndk/android-ndk-r9d-windows-x86_64.zip

# Variables setup

NDK:=$(realpath ${ANDROID_NDK})

ifndef ANDROID_NDK
define check-ndk
	$(error ANDROID_NDK is undefined);
endef
else
ifeq '${NDK}' ''
define check-ndk
	$(error ANDROID_NDK does not point to a directory);
endef
else
define check-ndk
endef
endif
endif

SRC_DIR:=$(shell pwd)

all: build/CMakeCache.txt

# --------
# Openssl
# --------

# This steps are based on the github project openssl1.0.1g-android
# https://github.com/aluvalasuman/openssl1.0.1g-android

openssl/openssl-1.0.1h.tar.gz:
	cd openssl && \
	wget http://www.openssl.org/source/openssl-1.0.1h.tar.gz

openssl/openssl-1.0.1h/Configure: openssl/openssl-1.0.1h.tar.gz
	cd openssl && \
	rm -rf openssl-1.0.1h && \
	tar xzf openssl-1.0.1h.tar.gz
	touch $@

openssl/r9d-9-armeabiv7/lib/libssl.a: openssl/openssl-1.0.1h/Configure
	$(call check-ndk)
	cd openssl/openssl-1.0.1h && \
	export ANDROID_NDK="${NDK}" && \
	. ../android_configure_armeabiv7.sh && \
	./Configure android no-shared --prefix=${SRC_DIR}/openssl/r9d-9-armeabiv7 --openssldir=openssl && \
	$(MAKE) all install_sw

# --------
# libiconv
# --------

# This steps are based on the blog post
# http://danilogiulianelli.blogspot.com/2012/12/how-to-cross-compile-libiconv-for.html

libiconv/libiconv-1.13.1.tar.gz:
	cd libiconv && \
	wget http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.13.1.tar.gz

libiconv/iconv-patched-stamp: libiconv/libiconv-1.13.1.tar.gz
	cd libiconv && \
	rm -rf libiconv-1.13.1 && \
	tar xzf libiconv-1.13.1.tar.gz && \
	patch -b -p0 < ./libiconv.patch && \
	touch iconv-patched-stamp

libiconv/libiconv-1.13.1/include/iconv.h: libiconv/iconv-patched-stamp
	cd libiconv/libiconv-1.13.1 && \
	./configure

libiconv/libs/armeabi-v7a/libiconv.so: libiconv/libiconv-1.13.1/include/iconv.h
	$(call check-ndk)
	cd libiconv/jni && \
	${ANDROID_NDK}/ndk-build

libiconv/r9d-9-armeabiv7a/include/iconv.h: libiconv/libiconv-1.13.1/include/iconv.h
	mkdir -p libiconv/r9d-9-armeabiv7a/include
	cp $< $@

libiconv/r9d-9-armeabiv7a/lib/libiconv.so: libiconv/libs/armeabi-v7a/libiconv.so
	mkdir -p libiconv/r9d-9-armeabiv7a/lib
	cp $< $@

# libiconv: libiconv/r9d-9-armeabiv7a/lib/libiconv.so libiconv/r9d-9-armeabiv7a/include/iconv.h

# -----
# Boost
# -----
# Uses the script from MysticTreeGames

Boost-for-Android/.git:
	git clone https://github.com/MysticTreeGames/Boost-for-Android.git

Boost-for-Android/patched-stamp: Boost-for-Android/.git boost-for-android.patch
	cd Boost-for-Android && \
	git checkout 8075d96cc9ef42d5c52d233b8ee42cb8421a2818 && \
	git checkout -- . && \
	patch -p1 < ../boost-for-android.patch && \
	touch patched-stamp

Boost-for-Android/build/lib/libboost_locale-gcc-mt-1_55.a: \
		libiconv/r9d-9-armeabiv7a/lib/libiconv.so \
		libiconv/r9d-9-armeabiv7a/include/iconv.h \
		Boost-for-Android/patched-stamp
	$(call check-ndk)
	cd Boost-for-Android && \
	./build-android.sh "${NDK}"

# -------------
# android-cmake
# -------------

android-cmake/android.toolchain.cmake:
	git clone https://github.com/taka-no-me/android-cmake.git

# ----------
# casablanca
# ----------

build/CMakeCache.txt: \
		android-cmake/android.toolchain.cmake \
		Boost-for-Android/build/lib/libboost_locale-gcc-mt-1_55.a \
		openssl/r9d-9-armeabiv7/lib/libssl.a \
		../Release/CMakeLists.txt
	$(call check-ndk)
	mkdir -p build
	cd build && \
	cmake ../../Release/ \
	-DCMAKE_TOOLCHAIN_FILE=../android-cmake/android.toolchain.cmake \
	-DANDROID_ABI=armeabi-v7a \
	-DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-4.8 \
	-DANDROID_STL=none \
	-DANDROID_STL_FORCE_FEATURES=ON \
	-DANDROID_NDK=${ANDROID_NDK} \
	-DANDROID_NATIVE_API_LEVEL=android-9 \
	-DANDROID_GOLD_LINKER=OFF \
	-DCMAKE_BUILD_TYPE=Debug
