Instructions to use Casablanca on iOS
=====================================

Casablanca depends on Boost and OpenSSL when used on iOS. It is a non-trivial
task to cross-compile libraries for iOS, however there are scripts available
online with nonrestrictive licenses to compile many popular libraries -- among
these libraries are Boost and OpenSSL.

This document will walk through the steps to build casablanca and its
dependencies into a form suitable for use with iOS applications.

For this walkthrough, we assume you are working within the root directory of the
casablanca project.

    $ git clone https://git01.codeplex.com/casablanca
    $ cd casablanca


Building OpenSSL
----------------

To build OpenSSL, use the script provided by the OpenSSL-for-iPhone project.

    $ git clone --depth=1 https://github.com/x2on/OpenSSL-for-iPhone.git
    $ cd OpenSSL-for-iPhone
    $ ./build-libssl.sh
    $ cd ..

After building the library, move the include files and libraries to
`casablanca/openssl-include` and `casablanca/openssl-ios/lib` respectively.

    $ mv OpenSSL-for-iPhone/include openssl-include
    $ mkdir openssl-ios
    $ mv OpenSSL-for-iPhone/lib openssl-ios/lib

This completes building OpenSSL.

[project link](https://github.com/x2on/OpenSSL-for-iPhone)


Building Boost
--------------

To build Boost, use the script provided by the boostoniphone project. The main
project author seems to have not continued maintaining the project, however
there are a few actively maintained forks. We recommend using the fork by Joseph
Galbraith.

    $ git clone https://git.gitorious.org/boostoniphone/galbraithjosephs-boostoniphone.git boostoniphone
    $ cd boostoniphone

The script `boost.sh` provided by the boostoniphone project has a variable at
the top of the file to specify which parts of boost need be compiled. This
variable must be changed to include the parts needed for Casablanca: threads,
signals, filesystem, regex, program_options, system.

    $ sed -e 's/\${BOOST_LIBS:=".*"}/\${BOOST_LIBS:="threads signals filesystem regex program_options system"}/g' -i .bak boost.sh
    $ ./boost.sh
    $ cd ..

Finally, the product framework must be moved into place.

    $ mv boostoniphone/ios/framework/boost.framework .

This completes building Boost.

[project link](https://gitorious.org/boostoniphone)
[fork link](https://gitorious.org/boostoniphone/galbraithjosephs-boostoniphone)


Preparing the Casablanca build
------------------------------

Casablanca uses CMake for cross-platform compatibility. To build on iOS, we
specifically use the toolchain file provided by the ios-cmake project.

    $ cd ios
    $ hg clone https://code.google.com/p/ios-cmake/ 
    $ cd ..

This completes the preparation for building Casablanca.

[project link](http://code.google.com/p/ios-cmake/)
[source link](http://ios-cmake.googlecode.com/files/ios-cmake.tar.gz)


Building Casablanca
-------------------

Now we are ready to build Casablanca for iOS. Invoke the ios-buildscripts
subproject in the usual CMake fashion:

    $ cd ios
    $ mkdir build.ios
    $ cd build.ios
    $ cmake .. -DCMAKE_BUILD_TYPE=Release
    $ make
    $ cd ..
    $ cd ..

This will take a while and produce appropriately lipo'd static libraries.


Using Casablanca
----------------
You will need to add/reference the following from your project:

  * ios/build.ios/libcasablanca.a
  * boost.framework
  * openssl-include
  * openssl-ios/lib/libcrypto.a
  * openssl-ios/lib/libssl.a
  * Release/include

This should allow you to reference and use casablanca from your C++ and
Objective-C++ source files. Note: you should change all .m files in your project
to .mm files, because even if the source file itself does not use Casablanca, it
is possible that some c++ code will be pulled in via header includes. To avoid
trivial errors later, it is easiest to simply rename all your project sources to
use '.mm'.
