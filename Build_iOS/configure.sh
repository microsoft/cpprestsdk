#!/bin/bash
set -e

if [ ! -e boost.framework ]
   then
       git clone -n https://github.com/faithfracture/Apple-Boost-BuildScript Apple-Boost-BuildScript
       pushd Apple-Boost-BuildScript
       git checkout 86f7570fceaef00846cc75f59c61474758fc65cb
       BOOST_LIBS="thread chrono filesystem regex system random" ./boost.sh
       popd
       mv Apple-Boost-BuildScript/build/boost/1.63.0/ios/framework/boost.framework .
       mv boost.framework/Versions/A/Headers boost.headers
       mkdir -p boost.framework/Versions/A/Headers
       mv boost.headers boost.framework/Versions/A/Headers/boost
fi

if [ ! -e openssl/lib/libcrypto.a ]
   then
       git clone --depth=1 https://github.com/x2on/OpenSSL-for-iPhone.git
       pushd OpenSSL-for-iPhone
       ./build-libssl.sh
       popd
       mkdir -p openssl/lib
       cp -r OpenSSL-for-iPhone/bin/iPhoneOS9.2-armv7.sdk/include openssl
       cp OpenSSL-for-iPhone/include/LICENSE openssl
       lipo -create -output openssl/lib/libssl.a OpenSSL-for-iPhone/bin/iPhone*/lib/libssl.a
       lipo -create -output openssl/lib/libcrypto.a OpenSSL-for-iPhone/bin/iPhone*/lib/libcrypto.a
fi

if [ ! -e ios-cmake/toolchain/iOS.cmake ]
   then
       git clone https://github.com/cristeab/ios-cmake.git
       pushd ios-cmake
       git apply ../fix_ios_cmake_compiler.patch
       popd
fi

mkdir -p build.ios
pushd build.ios
cmake .. -DCMAKE_BUILD_TYPE=Release
make
popd
echo "===="
echo "The final library is available in 'build.ios/libcpprest.a'"
