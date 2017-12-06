SHELL := /bin/bash
OPENSSL_VER = openssl-1.0.2k

all: armeabi-v7a/lib/libssl.a x86/lib/libssl.a

clean:
	rm -rf $(OPENSSL_VER)
	rm -rf $(OPENSSL_VER)-armeabi-v7a
	rm -rf armeabi-v7a
	rm -rf setenv-android-x86.sh

setenv-android.sh:
	wget https://wiki.openssl.org/images/7/70/Setenv-android.sh
	mv Setenv-android.sh setenv-android.sh
	chmod a+x setenv-android.sh

setenv-android-x86.sh: setenv-android.sh
	cp setenv-android.sh setenv-android-x86.sh.tmp
	sed -i -e 's/_ANDROID_EABI="arm-linux-androideabi-4.8"/_ANDROID_EABI="x86-4.8"/g' setenv-android-x86.sh.tmp
	sed -i -e 's/_ANDROID_ARCH=arch-arm/_ANDROID_ARCH=arch-x86/g' setenv-android-x86.sh.tmp
	mv setenv-android-x86.sh.tmp setenv-android-x86.sh

$(OPENSSL_VER).tar.gz:
	wget https://www.openssl.org/source/$(OPENSSL_VER).tar.gz

armeabi-v7a/lib/libssl.a: setenv-android.sh $(OPENSSL_VER).tar.gz
	[ -d "$(ANDROID_NDK_ROOT)" ]
	export ANDROID_NDK_ROOT="$(ANDROID_NDK_ROOT)"; \
	. ./setenv-android.sh; \
	( \
		set -e; \
		rm -rf $(OPENSSL_VER)/; \
		tar xzf $(OPENSSL_VER).tar.gz; \
		rm -rf $(OPENSSL_VER)-armeabi-v7a/ \
	) && \
	mv $(OPENSSL_VER) $(OPENSSL_VER)-armeabi-v7a && \
	cd $(OPENSSL_VER)-armeabi-v7a && \
	perl -pi -e 's/install: all install_docs install_sw/install: install_docs install_sw/g' Makefile.org && \
	./config shared -no-ssl2 -no-ssl3 -no-comp -no-hw -no-engine --openssldir="`pwd`/../armeabi-v7a" && \
	make depend && \
	make all && \
	make install CC=$${ANDROID_TOOLCHAIN}/arm-linux-androideabi-gcc RANLIB=$${ANDROID_TOOLCHAIN}/arm-linux-androideabi-ranlib

x86/lib/libssl.a: setenv-android-x86.sh $(OPENSSL_VER).tar.gz
	[ -d "$(ANDROID_NDK_ROOT)" ]
	export ANDROID_NDK_ROOT="$(ANDROID_NDK_ROOT)"; \
	. ./setenv-android-x86.sh; \
	( \
		set -e; \
		rm -rf $(OPENSSL_VER)/; \
		tar xzf $(OPENSSL_VER).tar.gz; \
		rm -rf $(OPENSSL_VER)-x86/ \
	) && \
	mv $(OPENSSL_VER) $(OPENSSL_VER)-x86 && \
	cd $(OPENSSL_VER)-x86 && \
	perl -pi -e 's/install: all install_docs install_sw/install: install_docs install_sw/g' Makefile.org && \
	./config shared -no-ssl2 -no-ssl3 -no-comp -no-hw -no-engine --openssldir="`pwd`/../x86" && \
	make depend && \
	make all && \
	make install CC=$${ANDROID_TOOLCHAIN}/i686-linux-android-gcc RANLIB=$${ANDROID_TOOLCHAIN}/i686-linux-android-ranlib
