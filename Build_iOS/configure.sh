#!/bin/bash
set -e

git clone https://gist.github.com/c629ae4c7168216a9856.git boostoniphone
pushd boostoniphone
git apply ../fix_boost_version.patch
./boost.sh
popd
mv boostoniphone/ios/framework/boost.framework .

git clone --depth=1 https://github.com/x2on/OpenSSL-for-iPhone.git
pushd OpenSSL-for-iPhone
./build-libssl.sh
popd
mkdir -p openssl/lib
cp -r OpenSSL-for-iPhone/bin/iPhoneOS8.2-armv7.sdk/include openssl
cp OpenSSL-for-iPhone/include/LICENSE openssl
lipo -create -output openssl/lib/libssl.a OpenSSL-for-iPhone/bin/iPhone*/lib/libssl.a
lipo -create -output openssl/lib/libcrypto.a OpenSSL-for-iPhone/bin/iPhone*/lib/libcrypto.a

# Now have a local copy of iOS-cmake to decouple from any changes (or lack of changes from the prior git repo)

mkdir build.ios
pushd build.ios
cmake .. -DCMAKE_BUILD_TYPE=Release
make
popd
echo "===="
echo "The final library is available in 'build.ios/libcpprest.a'"
