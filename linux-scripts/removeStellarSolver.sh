#!/bin/bash

sudo rm -r /usr/include/stellarsolver
sudo rm /usr/lib/stellarsolver.*
sudo rm /usr/bin/StellarSolverTester
sudo rm /usr/lib/pkgconfig/stellarsolver.pc
sudo rm $PKG_CONFIG_PATH/stellarsolver.pc
sudo rm $HOME/Desktop/StellarSolverTester.desktop