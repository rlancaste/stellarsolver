#!/bin/bash

#This gets the directory of the script, but note that the script is in a subdirectory, so to get the Repo's directory, we need ../
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/../

#This installs the dependencies
if [ -f /usr/lib/fedora-release ]; then
  sudo dnf -y install git cmake qt5 cfitsio-devel gsl-devel wcslib-devel
else
  sudo apt -y install g++ git cmake qt5-default libcfitsio-dev libgsl-dev wcslib-dev
fi

#This makes and installs the library
mkdir -p $DIR/build
cd $DIR/build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_QT5=ON -DBUILD_TESTER=ON $DIR
make -j $(expr $(nproc) + 2)
sudo make install

#This copies the icon into the pictures directory for the section below
cp $DIR/tester/StellarSolverIcon.png $HOME/Pictures/

# This will create a shortcut on the desktop for launching StellarSolver
##################
cat >$HOME/Desktop/StellarSolverTester.desktop <<-EOF
[Desktop Entry]
Version=1.0
Type=Application
Terminal=false
Icon[en_US]=$HOME/Pictures/StellarSolverIcon.png
Icon=$HOME/Pictures/StellarSolverIcon.png
Exec=/usr/bin/StellarSolverTester
Name[en_US]=StellarSolverTester
Name=StellarSolverTester
EOF
##################
