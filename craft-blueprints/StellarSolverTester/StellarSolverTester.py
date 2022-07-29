import glob
from xml.etree import ElementTree as et

import info


class subinfo(info.infoclass):
    def setTargets(self):
        self.description = 'StellarSolver Sextractor and Astrometry.net based Library Tester Program'
        self.svnTargets['master'] = "https://github.com/rlancaste/stellarsolver.git"
        for ver in ['2.4']:
            self.targets[ver] = 'https://github.com/rlancaste/stellarsolver/archive/refs/tags/%s.tar.gz' % ver
            self.archiveNames[ver] = "stellarsolver-tester-%s.tar.gz" % ver
            self.targetInstSrc[ver] = "stellarsolver-%s" % ver
        self.defaultTarget = '2.4'
    
    def setDependencies(self):
        self.runtimeDependencies["virtual/base"] = None
        self.runtimeDependencies["libs/qt5/qtbase"] = None
        self.runtimeDependencies["libs/gsl"] = None
        self.runtimeDependencies["libs/mman"] = None
        self.runtimeDependencies["libs/cfitsio"] = None
        self.runtimeDependencies["libs/zlib"] = None
        self.runtimeDependencies["boost-regex"] = None
        self.runtimeDependencies["libs/wcslib"] = None

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
