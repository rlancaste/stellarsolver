import glob
from xml.etree import ElementTree as et

import info


class subinfo(info.infoclass):
    def setTargets(self):
        self.description = 'SexySolver Sextractor and Astrometry.net based Library Tester Program'
        self.svnTargets['Latest'] = "https://github.com/rlancaste/sexysolver-tester.git"
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




from Package.CMakePackageBase import *


class Package(CMakePackageBase):
    def __init__(self):
        CMakePackageBase.__init__(self)
        root = CraftCore.standardDirs.craftRoot()
        craftLibDir = os.path.join(root,  'lib')
        self.subinfo.options.configure.args = "-DCMAKE_INSTALL_PREFIX=" + root + " -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MACOSX_RPATH=1 -DCMAKE_INSTALL_RPATH=" + craftLibDir

    def createPackage(self):
        self.defines["executable"] = "bin\\SexySolver.exe"
        self.defines["icon"] = os.path.join(self.packageDir(), "SexySolverInstallIcon.ico")
        if isinstance(self, AppxPackager):
              self.defines["display_name"] = "SexySolver"
        return TypePackager.createPackage(self)
