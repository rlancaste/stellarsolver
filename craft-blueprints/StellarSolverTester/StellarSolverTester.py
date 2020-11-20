import glob
from xml.etree import ElementTree as et

import info


class subinfo(info.infoclass):
    def setTargets(self):
        self.description = 'StellarSolver Sextractor and Astrometry.net based Library Tester Program'
        self.svnTargets['Latest'] = "https://github.com/rlancaste/stellarsolver.git"
        self.targetInstSrc['Latest'] = ""

        self.defaultTarget = 'Latest'
    
    def setDependencies(self):
        self.runtimeDependencies["virtual/base"] = "default"
        self.runtimeDependencies["libs/qt5/qtbase"] = "default"
        self.runtimeDependencies["libs/gsl"] = "default"
        self.runtimeDependencies["libs/mman"] = "default"
        self.runtimeDependencies["libs/cfitsio"] = "default"
        self.runtimeDependencies["libs/zlib"] = "default"
        self.runtimeDependencies["boost-regex"] = "default"
        self.runtimeDependencies["libs/wcslib"] = "default"




from Package.CMakePackageBase import *


class Package(CMakePackageBase):
    def __init__(self):
        CMakePackageBase.__init__(self)
        root = str(CraftCore.standardDirs.craftRoot())
        craftLibDir = os.path.join(root,  'lib')
        self.subinfo.options.configure.args = "-DCMAKE_INSTALL_PREFIX=" + root + " -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MACOSX_RPATH=1 -DBUILD_TESTER=ON -DCMAKE_INSTALL_RPATH=" + craftLibDir

    def createPackage(self):
        self.defines["executable"] = "bin\\StellarSolverTester.exe"
        self.defines["icon"] = os.path.join(self.packageDir(), "StellarSolverInstallIcon.ico")
        if isinstance(self, AppxPackager):
              self.defines["display_name"] = "StellarSolverTester"
        return TypePackager.createPackage(self)
