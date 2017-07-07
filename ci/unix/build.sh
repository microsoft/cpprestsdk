# build.sh

mkdir build.release
cd build.release
cmake ../Release -DCMAKE_BUILD_TYPE=Release
make -j4