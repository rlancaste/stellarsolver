#!/bin/bash

#This gets the directory of the script, but note that the script is in a subdirectory, so to get the Repo's directory, we need ../
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/../

#This installs the dependencies
if [ -f /usr/lib/fedora-release ]; then
  sudo dnf -y install git cmake qt5 cfitsio-devel gsl-devel wcslib-devel
else
    # Qt5 dependencies
    sudo apt -y install git cmake qt5-default libcfitsio-dev libgsl-dev wcslib-dev
    # Qt6 dependencies
    # sudo apt -y install git cmake qt6-base-dev libcfitsio-dev libgsl-dev wcslib-dev
fi

#This makes and installs the library
mkdir -p $DIR/build
cd $DIR/build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo $DIR
make -j $(expr $(nproc) + 2)
sudo make install
