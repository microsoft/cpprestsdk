# build.sh

SCRIPT_LOCATION=$(cd "$(dirname "$0")"; pwd)
. ${SCRIPT_LOCATION}/common.sh

mkdir ${BUILD_DIR}
cd ${BUILD_DIR}

CC="clang" CXX="clang++"                                                       \
    cmake ../Release -DCMAKE_BUILD_TYPE=Release                                \
                     -DBUILD_SAMPLES=no                                        \
                     -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}/${INSTALL_PREFIX}
make -j4
