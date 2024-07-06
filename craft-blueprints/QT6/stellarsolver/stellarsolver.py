import glob
import info
from Package.CMakePackageBase import CMakePackageBase

class subinfo(info.infoclass):
    def setTargets(self):
        self.description = "StellarSolver Star Extractor (SEP) and Astrometry.net based Library"
        self.svnTargets["master"] = "https://github.com/rlancaste/stellarsolver.git"
        for ver in ["2.6"]:
            self.targets[ver] = f"https://github.com/rlancaste/stellarsolver/archive/refs/tags/{ver}.tar.gz"
            self.archiveNames[ver] = f"stellarsolver-{ver}.tar.gz"
            self.targetInstSrc[ver] = f"stellarsolver-{ver}"
        self.defaultTarget = "2.6"
    
    def setDependencies(self):
        self.runtimeDependencies["virtual/base"] = None
        self.runtimeDependencies["libs/qt/qtbase"] = None
        self.runtimeDependencies["libs/gsl"] = None
        self.runtimeDependencies["libs/mman"] = None
        self.runtimeDependencies["libs/cfitsio"] = None
        self.runtimeDependencies["libs/zlib"] = None
        self.runtimeDependencies["boost-regex"] = None
        self.runtimeDependencies["libs/wcslib"] = None

class Package(CMakePackageBase):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.subinfo.options.configure.args += ["-DUSE_QT5=OFF"]
