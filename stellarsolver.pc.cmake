prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=@CMAKE_INSTALL_PREFIX@
libdir=@PKG_CONFIG_LIBDIR@
includedir=@INCLUDE_INSTALL_DIR@

Name: stellarsolver
Description: StellarSolver Internal Star Extraction and Plate Solving Library
URL: https://github.com/rlancaste/stellarsolver
Version: @StellarSolver_VERSION@
Libs: -L${libdir} -lstellarsolver
Libs.private: -lcfitsio -lwcslib -lpthread
Cflags: -I${includedir} -I${includedir}/libstellarsolver

