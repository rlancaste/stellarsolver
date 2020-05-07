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

cp ../SexySolverIcon.png $HOME/Pictures/

# This will create a shortcut on the desktop for launching SexySolver
##################
cat >$HOME/Desktop/SexySolverTester.desktop <<-EOF
[Desktop Entry]
Version=1.0
Type=Application
Terminal=false
Icon[en_US]=$HOME/Pictures/SexySolverIcon.png
Icon=$HOME/Pictures/SexySolverIcon.png
Exec=/usr/bin/SexySolverTester
Name[en_US]=SexySolverTester
Name=SexySolverTester
EOF
##################
