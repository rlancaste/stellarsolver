prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=@CMAKE_INSTALL_PREFIX@
libdir=@PKG_CONFIG_LIBDIR@
includedir=@INCLUDE_INSTALL_DIR@

Name: libsexysolver
Description: SexySolver Internal Sextraction and Plate Solving Library
URL: https://github.com/rlancaste/sexysolver-tester
Version: @SexySolver_VERSION@
Libs: -L${libdir} @PKG_CONFIG_LIBS@
Libs.private: -lz -lcfitsio
Cflags: -I${includedir} -I${includedir}/sexysolver

