#!/bin/bash
set -e

if [ ! -e boost.framework ]
then
    git clone https://github.com/faithfracture/Apple-Boost-BuildScript Apple-Boost-BuildScript
    pushd ./Apple-Boost-BuildScript
    git checkout 1b94ec2e2b5af1ee036d9559b96e70c113846392
    BOOST_LIBS="thread chrono filesystem regex system random" ./boost.sh -ios -tvos
    popd
    mv Apple-Boost-BuildScript/build/boost/1.67.0/ios/framework/boost.framework .
    mv boost.framework/Versions/A/Headers boost.headers
    mkdir -p boost.framework/Versions/A/Headers
    mv boost.headers boost.framework/Versions/A/Headers/boost
fi

if [ ! -e openssl/lib/libcrypto.a ]
then
    git clone --depth=1 https://github.com/x2on/OpenSSL-for-iPhone.git
    pushd OpenSSL-for-iPhone
    git checkout 10019638e80e8a8a5fc19642a840d8a69fac7349
    ./build-libssl.sh
    popd
    mkdir -p openssl/lib
    if [ -e OpenSSL-for-iPhone/bin/iPhoneOS11.4-arm64.sdk/include ]
    then
        cp -r OpenSSL-for-iPhone/bin/iPhoneOS11.4-arm64.sdk/include openssl
    elif [ -e OpenSSL-for-iPhone/bin/iPhoneOS12.0-arm64.sdk/include ]
        cp -r OpenSSL-for-iPhone/bin/iPhoneOS12.0-arm64.sdk/include openssl
    else
        echo 'Could not find OpenSSL for iPhone'
        exit 1
    fi
    cp OpenSSL-for-iPhone/include/LICENSE openssl
    lipo -create -output openssl/lib/libssl.a OpenSSL-for-iPhone/bin/iPhone*/lib/libssl.a
    lipo -create -output openssl/lib/libcrypto.a OpenSSL-for-iPhone/bin/iPhone*/lib/libcrypto.a
fi

if [ ! -e ios-cmake/ios.toolchain.cmake ]
then
    git clone https://github.com/leetal/ios-cmake
    pushd ios-cmake
    git checkout 6b30f4cfeab5567041d38e79507e642056fb9fd4
    popd
fi

mkdir -p build.release.ios
pushd build.release.ios
cmake -DCMAKE_BUILD_TYPE=Release ..
make
popd
echo "===="
echo "The final library is available in 'build.ios/libcpprest.a'"
