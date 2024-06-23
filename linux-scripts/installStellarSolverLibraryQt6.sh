#!/bin/bash

#This gets the directory of the script, but note that the script is in a subdirectory, so to get the Repo's directory, we need ../
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/../

#This installs the dependencies
if [ -f /usr/lib/fedora-release ]; then
  sudo dnf -y install git cmake qt6 cfitsio-devel gsl-devel wcslib-devel
else
  sudo apt -y install g++ git cmake qt6-base-dev libgl1-mesa-dev libcfitsio-dev libgsl-dev wcslib-dev
fi

#This makes and installs the library
mkdir -p $DIR/build
cd $DIR/build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_QT5=Off -DBUILD_TESTER=Off $DIR
make -j $(expr $(nproc) + 2)
sudo make install
