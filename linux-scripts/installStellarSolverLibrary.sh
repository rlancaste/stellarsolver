#!/bin/bash

if [ -f /usr/lib/fedora-release ]; then
  sudo dnf -y install git cmake qt5 cfitsio-devel gsl-devel wcslib-devel
else
  sudo apt -y install git cmake qt5-default libcfitsio-dev libgsl-dev wcslib-dev
fi

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo ../
make -j $(expr $(nproc) + 2)
sudo make install