# build.sh

mkdir build.release
cd build.release
CC="clang" CXX="clang++" cmake ../Release -DCMAKE_BUILD_TYPE=Release
make -j4
