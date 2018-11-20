#!/bin/bash
set -ev

ROOT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd /usr/src/gtest/
sudo cmake CMakeLists.txt
sudo make
sudo cp *.a /usr/lib/

cd $ROOT_DIR
git clone https://github.com/GENIVI/dlt-daemon.git
cd dlt-daemon
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_LIBDIR=lib -DWITH_DLT_CXX11_EXT=ON
make && sudo make install

cd $ROOT_DIR
git clone git://git.alsa-project.org/alsa-lib.git
cd alsa-lib
git checkout v1.1.7
autoreconf -i
./configure --with-pythonlibs="-lpthread -lm -ldl -lpython2.7" --with-pythonincludes=-I/usr/include/python2.7
make && sudo make install

cd $ROOT_DIR
mkdir build
cd build
cmake -DIAS_IS_HOST_BUILD=1 ../
make
