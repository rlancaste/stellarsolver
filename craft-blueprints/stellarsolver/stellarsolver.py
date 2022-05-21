import glob
import info


class subinfo(info.infoclass):
    def setTargets(self):
        self.description = 'StellarSolver Sextractor and Astrometry.net based Library'
        self.svnTargets['master'] = "https://github.com/rlancaste/stellarsolver.git"
        for ver in ['2.3']:
            self.targets[ver] = 'https://github.com/rlancaste/stellarsolver/archive/refs/tags/%s.tar.gz' % ver
            self.archiveNames[ver] = "stellarsolver-%s.tar.gz" % ver
            self.targetInstSrc[ver] = "stellarsolver-%s" % ver
        self.defaultTarget = '2.3'
    
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
        #root = CraftCore.standardDirs.craftRoot()
        #craftLibDir = os.path.join(root,  'lib')
        #self.subinfo.options.configure.args += f" -DCMAKE_MACOSX_RPATH=1 -DCMAKE_INSTALL_RPATH={craftLibDir}"
