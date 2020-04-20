PROJECT(sexysolver-tester C CXX)
cmake_minimum_required(VERSION 2.4.7)

cmake_policy(SET CMP0003 NEW)

# Timestamp build
string(TIMESTAMP SexySolver_BUILD_TS UTC)

# SexySolver Version 0.6
set (SexySolver_VERSION_MAJOR 0)
set (SexySolver_VERSION_MINOR 6)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

set(CMAKE_AUTOMOC ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13)
endif(APPLE)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")
include(GNUInstallDirs)

#This one is for craft
#include_directories("${CMAKE_INSTALL_PREFIX}/include")

include_directories( "${CMAKE_CURRENT_BINARY_DIR}")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/astrometry")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/astrometry/include")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/astrometry/include/astrometry")
include_directories("/usr/local/include")
include_directories("/usr/include")

set(config_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/os-features-test.c
    )

set(config_FN
    "${CMAKE_CURRENT_SOURCE_DIR}/astrometry/include/astrometry/os-features-config.h"
    )

file(REMOVE "${config_FN}")
if (WIN32)

    file(APPEND "${config_FN}" "#define NEED_DECLARE_QSORT_R 1\n")
    file(APPEND "${config_FN}" "#define NEED_QSORT_R 1\n")
    file(APPEND "${config_FN}" "#define NEED_SWAP_QSORT_R 0\n")

else(WIN32)

try_run(RUN_RESULT_2 COMPILE_SUCCESS_2 ${CMAKE_CURRENT_BINARY_DIR} ${config_SRCS} COMPILE_DEFINITIONS -DTEST_DECLARE_QSORT_R=ON)
if(COMPILE_SUCCESS_2 AND (RUN_RESULT_2 EQUAL 0))
    SET(VAR_2 0)
else()
    SET(VAR_2 1)
endif()
file(APPEND "${config_FN}" "#define NEED_DECLARE_QSORT_R ${VAR_2}\n")

try_run(RUN_RESULT_3 COMPILE_SUCCESS_3 ${CMAKE_CURRENT_BINARY_DIR} ${config_SRCS} COMPILE_DEFINITIONS -DTEST_QSORT_R=ON)
if(COMPILE_SUCCESS_3 AND (RUN_RESULT_3 EQUAL 0))
    SET(VAR_3 1)
else()
    SET(VAR_3 0)
endif()
file(APPEND "${config_FN}" "#define NEED_QSORT_R ${VAR_3}\n")

try_run(RUN_RESULT_4 COMPILE_SUCCESS_4 ${CMAKE_CURRENT_BINARY_DIR} ${config_SRCS} COMPILE_DEFINITIONS -DTEST_SWAP_QSORT_R=ON)
if(COMPILE_SUCCESS_4 AND (RUN_RESULT_4 EQUAL 0))
    SET(VAR_4 1)
else()
    SET(VAR_4 0)
endif()
file(APPEND "${config_FN}" "#define NEED_SWAP_QSORT_R ${VAR_4}\n")

endif(WIN32)

# We aren't using netpbm in this program, so set this to 0 no matter what
file(APPEND "${config_FN}" "#define HAVE_NETPBM 0")

find_package(CFITSIO REQUIRED)
find_package(GSL REQUIRED)
find_package(Qt5 5.4 REQUIRED COMPONENTS Gui Widgets Core Concurrent)

if (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)
  message(FATAL_ERROR "CFITSIO version too old, Please install cfitsio 3.x and try again. http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html")
endif (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)

if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

if (GSL_FOUND)
  include_directories(${GSL_INCLUDE_DIR})
endif (GSL_FOUND)

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an")
set(qfits_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/anqfits.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_card.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_convert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_header.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_table.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_tools.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_byteswap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_rw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/qfits-an/qfits_float.c
    )

add_library(qfits ${qfits_SRCS})
#install(TARGETS qfits LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(WIN32)
    find_package(Boost 1.45.0 COMPONENTS regex)
    target_link_libraries(qfits ${Boost_LIBRARIES})
endif(WIN32)

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd")
set(kd_SRCS
    #Internals
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdint_ddd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdint_fff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdint_ddu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdint_duu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdint_dds.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdint_dss.c
    #kd
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdtree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdtree_dim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdtree_mem.c
    #kd fits
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/kdtree_fits_io.c
    #dt
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/dualtree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/dualtree_rangesearch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/libkd/dualtree_nearestneighbour.c
    )

add_library(kd ${kd_SRCS})
target_link_libraries(kd ${CFITSIO_LIBRARIES})
#install(TARGETS ld LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util")
set(anbase_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/starutil.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/mathutil.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/bl-sort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/bl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/bt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/healpix-utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/healpix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/permutedsort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/ioutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/fileutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/os-features.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/an-endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/errors.c
    #${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/an-opts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/tic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/datalog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/sparsematrix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/coadd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/convolve-image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/resample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/intmap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/histogram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/histogram2d.c
    )

add_library(anbase ${anbase_SRCS})

if(NOT MSVC)
target_link_libraries(anbase -lpthread)
endif(NOT MSVC)
#install(TARGETS anbase LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

set(anutils_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/sip-utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/fit-wcs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/sip.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/anwcs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/wcs-resample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/gslutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/wcs-pv2sip.c

    #These get added if QFITS is to be included
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/fitsioutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/sip_qfits.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/fitstable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/fitsbin.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/fitsfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/tic.c
    )

add_library(anutils ${anutils_SRCS})
target_link_libraries(anutils anbase qfits ${GSL_LIBRARIES})
#install(TARGETS anutils LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

set(anfiles_SRCS
    #I think thiese only get added if QFITS is to be included
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/multiindex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/index.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/codekd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/starkd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/rdlist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/xylist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/starxy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/qidxfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/quadfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/scamp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/scamp-catalog.c
   # ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/tabsort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/wcs-xy2rd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/util/wcs-rd2xy.c
        )

add_library(anfiles ${anfiles_SRCS})
target_link_libraries(anfiles anbase anutils qfits kd )
#install(TARGETS anfiles LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind")
set(engine_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/blindutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/blind.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/solver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/quad-utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/matchfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/matchobj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/solvedclient.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/solvedfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/tweak2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/astrometry/blind/tweak.c
    )
add_library(astrometry ${engine_SRCS})
target_link_libraries(astrometry anbase anutils anfiles)
if(WIN32)
target_link_libraries(astrometry wsock32)
endif(WIN32)
#install(TARGETS astrometry LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

set (sep_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/sep/analyse.c
   ${CMAKE_CURRENT_SOURCE_DIR}/sep/aperture.c
   ${CMAKE_CURRENT_SOURCE_DIR}/sep/background.c
   ${CMAKE_CURRENT_SOURCE_DIR}/sep/convolve.c
   ${CMAKE_CURRENT_SOURCE_DIR}/sep/deblend.c
   ${CMAKE_CURRENT_SOURCE_DIR}/sep/extract.c
   ${CMAKE_CURRENT_SOURCE_DIR}/sep/lutz.c
   ${CMAKE_CURRENT_SOURCE_DIR}/sep/util.c
    )

add_library(sep ${sep_SRCS})
target_link_libraries(sep ${CFITSIO_LIBRARIES})
#install(TARGETS sep LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

set(SexySolver_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/imagelabel.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/externalsextractorsolver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/sexysolver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stretch.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/dms.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bayer.c
   )

qt5_wrap_ui(SexySolverui_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.ui
    )

If(APPLE)
set(MACOSX_BUNDLE_ICON_FILE SexySolverIcon.icns)
set(SexySolverApp_ICON SexySolverIcon.icns)
set_source_files_properties(${SexySolverApp_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif(APPLE)

if(WIN32)
set(SexySolverApp_ICON windowsicon.rc)
endif(WIN32)

add_executable(SexySolver ${SexySolver_SRCS} ${SexySolverui_SRCS} ${SexySolverApp_ICON})

if(APPLE)
    SET_TARGET_PROPERTIES(SexySolver PROPERTIES MACOSX_BUNDLE TRUE)
endif(APPLE)

target_link_libraries(SexySolver sep astrometry)

target_link_libraries(SexySolver
    ${CFITSIO_LIBRARIES}
    ${GSL_LIBRARIES}

    Qt5::Gui
    Qt5::Widgets
    Qt5::Core
    Qt5::Concurrent
    )

if(APPLE)
install(TARGETS SexySolver
    BUNDLE DESTINATION ${CMAKE_INSTALL_PREFIX}
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}
    )
else(APPLE)
	#installation for Linux
	install(TARGETS SexySolver RUNTIME DESTINATION bin)
endif(APPLE)





