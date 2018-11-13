#!/usr/bin/env bash
set -e

ABS_PATH="`dirname \"$0\"`"                 # relative
ABS_PATH="`( cd \"${ABS_PATH}\" && pwd )`"  # absolutized and normalized
# Make sure that the path to this file exists and can be retrieved!
if [ -z "${ABS_PATH}" ]; then
  echo "Could not fetch the ABS_PATH."
  exit 1
fi

## Configuration
DEFAULT_BOOST_VERSION=1.67.0
DEFAULT_OPENSSL_VERSION=1.0.2o
BOOST_VERSION=${BOOST_VERSION:-${DEFAULT_BOOST_VERSION}}
OPENSSL_VERSION=${OPENSSL_VERSION:-${DEFAULT_OPENSSL_VERSION}}
CPPRESTSDK_BUILD_TYPE=${CPPRESTSDK_BUILD_TYPE:-Release}

############################ No need to edit anything below this line

## Set some needed variables
IOS_SDK_VERSION=`xcrun --sdk iphoneos --show-sdk-version`

## Buildsteps below

## Fetch submodules just in case
git submodule update --init

## Build Boost

if [ ! -e $ABS_PATH/boost.framework ]; then
    if [ ! -d "${ABS_PATH}/Apple-Boost-BuildScript" ]; then
        git clone https://github.com/faithfracture/Apple-Boost-BuildScript ${ABS_PATH}/Apple-Boost-BuildScript
    fi
    pushd ${ABS_PATH}/Apple-Boost-BuildScript
    git checkout 1b94ec2e2b5af1ee036d9559b96e70c113846392
    BOOST_LIBS="thread chrono filesystem regex system random" ./boost.sh -ios -tvos --boost-version $BOOST_VERSION
    popd
    mv ${ABS_PATH}/Apple-Boost-BuildScript/build/boost/${BOOST_VERSION}/ios/framework/boost.framework ${ABS_PATH}
    mv ${ABS_PATH}/boost.framework/Versions/A/Headers ${ABS_PATH}/boost.headers
    mkdir -p ${ABS_PATH}/boost.framework/Versions/A/Headers
    mv ${ABS_PATH}/boost.headers ${ABS_PATH}/boost.framework/Versions/A/Headers/boost
fi

## Build OpenSSL

if [ ! -e ${ABS_PATH}/openssl/lib/libcrypto.a ]; then
    if [ ! -d "${ABS_PATH}/OpenSSL-for-iPhone" ]; then
       git clone --depth=1 https://github.com/x2on/OpenSSL-for-iPhone.git ${ABS_PATH}/OpenSSL-for-iPhone
    fi
    pushd ${ABS_PATH}/OpenSSL-for-iPhone
    git checkout 10019638e80e8a8a5fc19642a840d8a69fac7349
    ./build-libssl.sh --version=${OPENSSL_VERSION}
    popd
    mkdir -p ${ABS_PATH}/openssl/lib
    if [ -e ${ABS_PATH}/OpenSSL-for-iPhone/bin/iPhoneOS${IOS_SDK_VERSION}-arm64.sdk/include ]
    then
        cp -r ${ABS_PATH}/OpenSSL-for-iPhone/bin/iPhoneOS${IOS_SDK_VERSION}-arm64.sdk/include ${ABS_PATH}/openssl
    else
        echo 'Could not find OpenSSL for iPhone'
        exit 1
    fi
    cp ${ABS_PATH}/OpenSSL-for-iPhone/include/LICENSE ${ABS_PATH}/openssl
    lipo -create -output ${ABS_PATH}/openssl/lib/libssl.a ${ABS_PATH}/OpenSSL-for-iPhone/bin/iPhone*/lib/libssl.a
    lipo -create -output ${ABS_PATH}/openssl/lib/libcrypto.a ${ABS_PATH}/OpenSSL-for-iPhone/bin/iPhone*/lib/libcrypto.a
fi

## Fetch CMake toolchain

if [ ! -e ${ABS_PATH}/ios-cmake/ios.toolchain.cmake ]; then
    if [ ! -d "${ABS_PATH}/ios-cmake" ]; then
        git clone https://github.com/leetal/ios-cmake ${ABS_PATH}/ios-cmake
    fi
    pushd ${ABS_PATH}/ios-cmake
    git checkout 2.1.2
    popd
fi

## Build CPPRestSDK

mkdir -p ${ABS_PATH}/build.${CPPRESTSDK_BUILD_TYPE}.ios
pushd ${ABS_PATH}/build.${CPPRESTSDK_BUILD_TYPE}.ios
cmake -DCMAKE_BUILD_TYPE=${CPPRESTSDK_BUILD_TYPE} ..
make
popd
printf "\n\n===================================================================================\n"
echo ">>>> The final library is available in 'build.${CPPRESTSDK_BUILD_TYPE}.ios/libcpprest.a'"
printf "===================================================================================\n\n"
