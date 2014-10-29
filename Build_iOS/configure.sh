#!/bin/bash
set -e

git clone https://git.gitorious.org/boostoniphone/galbraithjosephs-boostoniphone.git boostoniphone
pushd boostoniphone
sed -e 's/\${IPHONE_SDKVERSION:=7.0}/\${IPHONE_SDKVERSION:=8.0}/' -i .bak boost.sh
sed -e 's/\${BOOST_LIBS:=".*"}/\${BOOST_LIBS:="random thread filesystem regex locale system date_time"}/g' -i .bak boost.sh
./boost.sh
pushd ios/framework/boost.framework/Versions/A
mkdir Headers2
mv Headers Headers2/boost
mv Headers2 Headers
popd
popd
mv boostoniphone/ios/framework/boost.framework .

git clone --depth=1 https://github.com/x2on/OpenSSL-for-iPhone.git
pushd OpenSSL-for-iPhone
./build-libssl.sh
popd
mkdir openssl
mv OpenSSL-for-iPhone/include openssl
mv OpenSSL-for-iPhone/lib openssl

hg clone https://code.google.com/p/ios-cmake/
mkdir build.ios
pushd build.ios
cmake .. -DCMAKE_BUILD_TYPE=Release
make
popd
echo "===="
echo "The final library is available in 'build.ios/libcasablanca.a'"
