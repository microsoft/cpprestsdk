#!/bin/sh
# Copyright (C) 2010 Mystic Tree Games
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: Moritz "Moss" Wundke (b.thax.dcg@gmail.com)
#
# <License>
#
# Build boost for android completly. It will download boost 1.45.0
# prepare the build system and finally build it for android

# Add common build methods
. `dirname $0`/build-common.sh

# -----------------------
# Command line arguments
# -----------------------

BOOST_VER1=1
BOOST_VER2=53
BOOST_VER3=0
register_option "--boost=<version>" boost_version "Boost version to be used, one of {1.55.0, 1.54.0, 1.53.0, 1.49.0, 1.48.0, 1.45.0}, default is 1.53.0."
boost_version()
{
  if [ "$1" = "1.55.0" ]; then
    BOOST_VER1=1
    BOOST_VER2=55
    BOOST_VER3=0
  elif [ "$1" = "1.54.0" ]; then
    BOOST_VER1=1
    BOOST_VER2=54
    BOOST_VER3=0
  elif [ "$1" = "1.53.0" ]; then
    BOOST_VER1=1
    BOOST_VER2=53
    BOOST_VER3=0
  elif [ "$1" = "1.49.0" ]; then
    BOOST_VER1=1
    BOOST_VER2=49
    BOOST_VER3=0
  elif [ "$1" = "1.48.0" ]; then
    BOOST_VER1=1
    BOOST_VER2=48
    BOOST_VER3=0
  elif [ "$1" = "1.45.0" ]; then
    BOOST_VER1=1
    BOOST_VER2=45
    BOOST_VER3=0
  else
    echo "Unsupported boost version '$1'."
    exit 1
  fi
}

register_option "--toolchain=<toolchain>" select_toolchain "Select a toolchain. To see available execute ls -l ANDROID_NDK/toolchains."
select_toolchain () {
    TOOLCHAIN=$1
}

CLEAN=no
register_option "--clean"    do_clean     "Delete all previously downloaded and built files, then exit."
do_clean () {	CLEAN=yes; }

DOWNLOAD=no
register_option "--download" do_download  "Only download required files and clean up previus build. No build will be performed."

do_download ()
{
	DOWNLOAD=yes
	# Clean previus stuff too!
	CLEAN=yes
}

#LIBRARIES=--with-libraries=date_time,filesystem,program_options,regex,signals,system,thread,iostreams
LIBRARIES=
register_option "--with-libraries=<list>" do_with_libraries "Comma separated list of libraries to build."
do_with_libraries () { 
  for lib in $(echo $1 | tr ',' '\n') ; do LIBRARIES="--with-$lib ${LIBRARIES}"; done 
}

register_option "--without-libraries=<list>" do_without_libraries "Comma separated list of libraries to exclude from the build."
do_without_libraries () {	LIBRARIES="--without-libraries=$1"; }
do_without_libraries () { 
  for lib in $(echo $1 | tr ',' '\n') ; do LIBRARIES="--without-$lib ${LIBRARIES}"; done 
}

register_option "--prefix=<path>" do_prefix "Prefix to be used when installing libraries and includes."
do_prefix () {
    if [ -d $1 ]; then
        PREFIX=$1;
    fi
}

PROGRAM_PARAMETERS="<ndk-root>"
PROGRAM_DESCRIPTION=\
"       Boost For Android\n"\
"Copyright (C) 2010 Mystic Tree Games\n"\

extract_parameters $@

echo "Building boost version: $BOOST_VER1.$BOOST_VER2.$BOOST_VER3"

# -----------------------
# Build constants
# -----------------------

BOOST_DOWNLOAD_LINK="http://downloads.sourceforge.net/project/boost/boost/$BOOST_VER1.$BOOST_VER2.$BOOST_VER3/boost_${BOOST_VER1}_${BOOST_VER2}_${BOOST_VER3}.tar.bz2?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fboost%2Ffiles%2Fboost%2F${BOOST_VER1}.${BOOST_VER2}.${BOOST_VER3}%2F&ts=1291326673&use_mirror=garr"
BOOST_TAR="boost_${BOOST_VER1}_${BOOST_VER2}_${BOOST_VER3}.tar.bz2"
BOOST_DIR="boost_${BOOST_VER1}_${BOOST_VER2}_${BOOST_VER3}"
BUILD_DIR="./build/"

# -----------------------

if [ $CLEAN = yes ] ; then
	echo "Cleaning: $BUILD_DIR"
	rm -f -r $PROGDIR/$BUILD_DIR
	
	echo "Cleaning: $BOOST_DIR"
	rm -f -r $PROGDIR/$BOOST_DIR
	
	echo "Cleaning: $BOOST_TAR"
	rm -f $PROGDIR/$BOOST_TAR

	echo "Cleaning: logs"
	rm -f -r logs
	rm -f build.log

  [ "$DOWNLOAD" = "yes" ] || exit 0
fi

# It is almost never desirable to have the boost-X_Y_Z directory from
# previous builds as this script doesn't check in which state it's
# been left (bootstrapped, patched, built, ...). Unless maybe during
# a debug, in which case it's easy for a developer to comment out
# this code.

if [ -d "$PROGDIR/$BOOST_DIR" ]; then
	echo "Cleaning: $BOOST_DIR"
	rm -f -r $PROGDIR/$BOOST_DIR
fi

if [ -d "$PROGDIR/$BUILD_DIR" ]; then
	echo "Cleaning: $BUILD_DIR"
	rm -f -r $PROGDIR/$BUILD_DIR
fi


AndroidNDKRoot=$PARAMETERS
if [ -z "$AndroidNDKRoot" ] ; then
  if [ -n "${ANDROID_BUILD_TOP}" ]; then # building from Android sources
    AndroidNDKRoot="${ANDROID_BUILD_TOP}/prebuilts/ndk/current"
    export AndroidSourcesDetected=1
  elif [ -z "`which ndk-build`" ]; then
    dump "ERROR: You need to provide a <ndk-root>!"
    exit 1
  else
    AndroidNDKRoot=`which ndk-build`
    AndroidNDKRoot=`dirname $AndroidNDKRoot`
  fi
  echo "Using AndroidNDKRoot = $AndroidNDKRoot"
else
  # User passed the NDK root as a parameter. Make sure the directory
  # exists and make it an absolute path.
  if [ ! -f "$AndroidNDKRoot/ndk-build" ]; then
    dump "ERROR: $AndroidNDKRoot is not a valid NDK root"
    exit 1
  fi
  AndroidNDKRoot=$(cd $AndroidNDKRoot; pwd -P)
fi
export AndroidNDKRoot

# Check platform patch
case "$HOST_OS" in
    linux)
        PlatformOS=linux
        ;;
    darwin|freebsd)
        PlatformOS=darwin
        ;;
    windows|cygwin)
        PlatformOS=windows
        ;;
    *)  # let's play safe here
        PlatformOS=linux
esac

NDK_SOURCE_PROPERTIES=$AndroidNDKRoot"/source.properties"
NDK_RELEASE_FILE=$AndroidNDKRoot"/RELEASE.TXT"
if [ -f "${NDK_SOURCE_PROPERTIES}" ]; then
    version=$(grep -i '^Pkg.Revision =' $NDK_SOURCE_PROPERTIES | cut -f2- -d=)
    NDK_RN=$(echo $version | awk -F. '{print $1}')
elif [ -f "${NDK_RELEASE_FILE}" ]; then
    NDK_RN=`cat $NDK_RELEASE_FILE | sed 's/^r\(.*\)$/\1/g'`
elif [ -n "${AndroidSourcesDetected}" ]; then
    if [ -f "${ANDROID_BUILD_TOP}/ndk/docs/CHANGES.html" ]; then
        NDK_RELEASE_FILE="${ANDROID_BUILD_TOP}/ndk/docs/CHANGES.html"
        NDK_RN=`grep "android-ndk-" "${NDK_RELEASE_FILE}" | head -1 | sed 's/^.*r\(.*\)$/\1/'`
    elif [ -f "${ANDROID_BUILD_TOP}/ndk/docs/text/CHANGES.text" ]; then
        NDK_RELEASE_FILE="${ANDROID_BUILD_TOP}/ndk/docs/text/CHANGES.text"
        NDK_RN=`grep "android-ndk-" "${NDK_RELEASE_FILE}" | head -1 | sed 's/^.*r\(.*\)$/\1/'`
    else
        dump "ERROR: can not find ndk version"
        exit 1
    fi
else
    dump "ERROR: can not find ndk version"
    exit 1
fi

echo "Detected Android NDK version $NDK_RN"

case "$NDK_RN" in
	4*)
		TOOLCHAIN=${TOOLCHAIN:-arm-eabi-4.4.0}
		CXXPATH=$AndroidNDKRoot/build/prebuilt/$PlatformOS-x86/${TOOLCHAIN}/bin/arm-eabi-g++
		TOOLSET=gcc-androidR4
		;;
	5*)
		TOOLCHAIN=${TOOLCHAIN:-arm-linux-androideabi-4.4.3}
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/$PlatformOS-x86/bin/arm-linux-androideabi-g++
		TOOLSET=gcc-androidR5
		;;
	7-crystax-5.beta3)
		TOOLCHAIN=${TOOLCHAIN:-arm-linux-androideabi-4.6.3}
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/$PlatformOS-x86/bin/arm-linux-androideabi-g++
		TOOLSET=gcc-androidR7crystax5beta3
		;;
	8)
		TOOLCHAIN=${TOOLCHAIN:-arm-linux-androideabi-4.4.3}
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/$PlatformOS-x86/bin/arm-linux-androideabi-g++
		TOOLSET=gcc-androidR8
		;;
	8b|8c|8d)
		TOOLCHAIN=${TOOLCHAIN:-arm-linux-androideabi-4.6}
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/$PlatformOS-x86/bin/arm-linux-androideabi-g++
		TOOLSET=gcc-androidR8b
		;;
	8e|9|9b|9c|9d)
		TOOLCHAIN=${TOOLCHAIN:-arm-linux-androideabi-4.6}
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/$PlatformOS-x86/bin/arm-linux-androideabi-g++
		TOOLSET=gcc-androidR8e
		;;
	"8e (64-bit)")
		TOOLCHAIN=${TOOLCHAIN:-arm-linux-androideabi-4.6}
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/${PlatformOS}-x86_64/bin/arm-linux-androideabi-g++
		TOOLSET=gcc-androidR8e
		;;
	"9 (64-bit)"|"9b (64-bit)"|"9c (64-bit)"|"9d (64-bit)")
		TOOLCHAIN=${TOOLCHAIN:-arm-linux-androideabi-4.6}
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/${PlatformOS}-x86_64/bin/arm-linux-androideabi-g++
		TOOLSET=gcc-androidR8e
		;;
	"10 (64-bit)")
		TOOLCHAIN=llvm-3.4
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/${PlatformOS}-x86_64/bin/clang++
		TOOLSET=clang-androidR8e
                ;;
	"10e-rc4 (64-bit)"|"10e (64-bit)")
		TOOLCHAIN=llvm-3.6
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/${PlatformOS}-x86_64/bin/clang++
		TOOLSET=clang-androidR8e
                ;;
	11)
		TOOLCHAIN=llvm
		CXXPATH=$AndroidNDKRoot/toolchains/${TOOLCHAIN}/prebuilt/${PlatformOS}-x86_64/bin/clang++
		TOOLSET=clang-androidR8e
		;;
	*)
		echo "Undefined or not supported Android NDK version!"
		exit 1
esac

if [ -n "${AndroidSourcesDetected}" ]; then # Overwrite CXXPATH if we are building from Android sources
    CXXPATH="${ANDROID_TOOLCHAIN}/arm-linux-androideabi-g++"
fi

echo Building with TOOLSET=$TOOLSET CXXPATH=$CXXPATH CXXFLAGS=$CXXFLAGS | tee $PROGDIR/build.log

# Check if the ndk is valid or not
if [ ! -f $CXXPATH ]
then
	echo "Cannot find C++ compiler at: $CXXPATH"
	exit 1
fi

# -----------------------
# Download required files
# -----------------------

# Downalod and unzip boost in a temporal folder and
if [ ! -f $BOOST_TAR ]
then
	echo "Downloading boost ${BOOST_VER1}.${BOOST_VER2}.${BOOST_VER3} please wait..."
	prepare_download
	download_file $BOOST_DOWNLOAD_LINK $PROGDIR/$BOOST_TAR
fi

if [ ! -f $PROGDIR/$BOOST_TAR ]
then
	echo "Failed to download boost! Please download boost ${BOOST_VER1}.${BOOST_VER2}.${BOOST_VER3} manually\nand save it in this directory as $BOOST_TAR"
	exit 1
fi

if [ ! -d $PROGDIR/$BOOST_DIR ]
then
	echo "Unpacking boost"
	if [ "$OPTION_PROGRESS" = "yes" ] ; then
		pv $PROGDIR/$BOOST_TAR | tar xjf - -C $PROGDIR
	else
		tar xjf $PROGDIR/$BOOST_TAR
	fi
fi

if [ $DOWNLOAD = yes ] ; then
	echo "All required files has been downloaded and unpacked!"
	exit 0
fi

# ---------
# Bootstrap
# ---------
if [ ! -f ./$BOOST_DIR/bjam ]
then
  # Make the initial bootstrap
  echo "Performing boost bootstrap"

  cd $BOOST_DIR 
  case "$HOST_OS" in
    windows)
        cmd //c "bootstrap.bat" 2>&1 | tee -a $PROGDIR/build.log
        ;;
    *)  # Linux and others
        ./bootstrap.sh 2>&1 | tee -a $PROGDIR/build.log
    esac


  if [ $? != 0 ] ; then
  	dump "ERROR: Could not perform boostrap! See $TMPLOG for more info."
  	exit 1
  fi
  cd $PROGDIR
  
  # -------------------------------------------------------------
  # Patching will be done only if we had a successfull bootstrap!
  # -------------------------------------------------------------

  # Apply patches to boost
  BOOST_VER=${BOOST_VER1}_${BOOST_VER2}_${BOOST_VER3}
  PATCH_BOOST_DIR=$PROGDIR/patches/boost-${BOOST_VER}

  cp configs/user-config-boost-${BOOST_VER}.jam $BOOST_DIR/tools/build/v2/user-config.jam

  for dir in $PATCH_BOOST_DIR; do
    if [ ! -d "$dir" ]; then
      echo "Could not find directory '$dir' while looking for patches"
      exit 1
    fi

    PATCHES=`(cd $dir && ls *.patch | sort) 2> /dev/null`

    if [ -z "$PATCHES" ]; then
      echo "No patches found in directory '$dir'"
      exit 1
    fi

    for PATCH in $PATCHES; do
      PATCH=`echo $PATCH | sed -e s%^\./%%g`
      SRC_DIR=$PROGDIR/$BOOST_DIR
      PATCHDIR=`dirname $PATCH`
      PATCHNAME=`basename $PATCH`
      log "Applying $PATCHNAME into $SRC_DIR/$PATCHDIR"
      cd $SRC_DIR && patch -p1 < $dir/$PATCH && cd $PROGDIR
      if [ $? != 0 ] ; then
        dump "ERROR: Patch failure !! Please check your patches directory!"
        dump "       Try to perform a clean build using --clean ."
        dump "       Problem patch: $dir/$PATCHNAME"
        exit 1
      fi
    done
  done
fi

echo "# ---------------"
echo "# Build using NDK"
echo "# ---------------"

# Build boost for android
echo "Building boost for android"
(
  cd $BOOST_DIR

  echo "Adding pathname: `dirname $CXXPATH`"
  # `AndroidBinariesPath` could be used by user-config-boost-*.jam
  export AndroidBinariesPath=`dirname $CXXPATH`
  export PATH=$AndroidBinariesPath:$PATH
  export AndroidNDKRoot
  export PlatformOS
  export NO_BZIP2=1

  cxxflags=""
  for flag in $CXXFLAGS; do cxxflags="$cxxflags cxxflags=$flag"; done

  { ./bjam -q                         \
         target-os=linux              \
         toolset=$TOOLSET             \
         $cxxflags                    \
         link=static                  \
         threading=multi              \
         --layout=versioned           \
         --prefix="./../$BUILD_DIR/"  \
         $LIBRARIES                   \
         release debug install 2>&1   \
         || { dump "ERROR: Failed to build boost for android!" ; exit 1 ; }
  } | tee -a $PROGDIR/build.log

  # PIPESTATUS variable is defined only in Bash, and we are using /bin/sh, which is not Bash on newer Debian/Ubuntu
)

dump "Done!"

if [ $PREFIX ]; then
    echo "Prefix set, copying files to $PREFIX"
    cp -r $PROGDIR/$BUILD_DIR/lib $PREFIX
    cp -r $PROGDIR/$BUILD_DIR/include $PREFIX
fi
