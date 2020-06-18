#!/bin/bash

sudo rm -r /usr/include/stellarsolver
sudo rm /usr/lib/stellarsolver.*
sudo rm /usr/bin/StellarSolverTester
sudo rm /usr/lib/pkgconfig/stellarsolver.pc
sudo rm $PKG_CONFIG_PATH/stellarsolver.pc
sudo rm $HOME/Desktop/StellarSolverTester.desktop

# Just in case the user had sexysolver before the rename
sudo rm -r /usr/include/sexysolver
sudo rm /usr/lib/sexysolver.*
sudo rm /usr/bin/SexySolverTester
sudo rm /usr/lib/pkgconfig/sexysolver.pc
sudo rm $PKG_CONFIG_PATH/sexysolver.pc
sudo rm $HOME/Desktop/SexySolverTester.desktop