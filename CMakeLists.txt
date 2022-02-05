PROJECT(StellarSolver C CXX)
cmake_minimum_required(VERSION 3.0)

#cmake_policy(SET CMP0003 NEW)

# Timestamp build
string(TIMESTAMP StellarSolver_BUILD_TS UTC)

# StellarSolver Version 1.9
set (StellarSolver_VERSION_MAJOR 1)
set (StellarSolver_VERSION_MINOR 9)

set (StellarSolver_SOVERSION "${StellarSolver_VERSION_MAJOR}")
set (StellarSolver_VERSION ${StellarSolver_VERSION_MAJOR}.${StellarSolver_VERSION_MINOR})
set (StellarSolver_VERSION_STRING "${StellarSolver_VERSION_MAJOR}.${StellarSolver_VERSION_MINOR}")

# Exports
set(TARGET_EXPORT_NAME ${PROJECT_NAME}Targets)
set(TARGET_EXPORT_CONFIG ${PROJECT_NAME}Config)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13)
endif(APPLE)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")
include(GNUInstallDirs)

include_directories( "${CMAKE_CURRENT_BINARY_DIR}")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/tester")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/include")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/include/astrometry")

set(config_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/os-features-test.c
    )

set(config_FN
    "${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/include/astrometry/os-features-config.h"
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

option(BUILD_TESTER "Build stellarsolver tester program, instead of just the library" Off)

find_package(CFITSIO REQUIRED)
find_package(GSL REQUIRED)
find_package(WCSLIB REQUIRED)
find_package(Qt5 5.4 REQUIRED COMPONENTS Gui Widgets Core Concurrent Network)

if(WIN32)
    find_package(Boost 1.45.0 COMPONENTS regex)
endif(WIN32)

if (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)
  message(FATAL_ERROR "CFITSIO version too old, Please install cfitsio 3.x and try again. http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html")
endif (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)

if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

if (GSL_FOUND)
  include_directories(${GSL_INCLUDE_DIR})
endif (GSL_FOUND)

if(WCSLIB_FOUND)
    include_directories( ${WCSLIB_INCLUDE_DIR} )
endif(WCSLIB_FOUND)

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an")
set(qfits_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/anqfits.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_card.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_convert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_header.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_table.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_tools.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_byteswap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_rw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/qfits-an/qfits_float.c
    )

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd")
set(kd_SRCS
    #Internals
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdint_ddd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdint_fff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdint_ddu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdint_duu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdint_dds.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdint_dss.c
    #kd
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdtree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdtree_dim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdtree_mem.c
    #kd fits
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/kdtree_fits_io.c
    #dt
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/dualtree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/dualtree_rangesearch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/libkd/dualtree_nearestneighbour.c
    )

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util")
set(anbase_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/starutil.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/mathutil.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/bl-sort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/bl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/bt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/healpix-utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/healpix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/permutedsort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/ioutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/fileutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/os-features.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/an-endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/errors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/tic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/datalog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/sparsematrix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/coadd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/convolve-image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/resample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/intmap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/histogram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/histogram2d.c
    )

set(anutils_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/sip-utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/fit-wcs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/sip.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/anwcs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/wcs-resample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/gslutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/wcs-pv2sip.c

    #These get added if QFITS is to be included
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/fitsioutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/sip_qfits.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/fitstable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/fitsbin.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/fitsfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/tic.c
    )

set(anfiles_SRCS
    #I think thiese only get added if QFITS is to be included
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/multiindex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/index.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/codekd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/starkd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/rdlist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/xylist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/starxy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/qidxfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/quadfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/scamp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/scamp-catalog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/wcs-xy2rd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/util/wcs-rd2xy.c
        )

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind")
set(engine_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/blindutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/blind.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/solver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/quad-utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/matchfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/matchobj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/solvedclient.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/solvedfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/tweak2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/blind/tweak.c
    )

set (sep_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sep/analyse.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sep/aperture.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sep/background.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sep/convolve.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sep/deblend.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sep/extract.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sep/lutz.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sep/util.cpp
    )

set(StellarSolver_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/parameters.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sextractorsolver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/internalsextractorsolver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/externalsextractorsolver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/onlinesolver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/stellarsolver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometrylogger.cpp
   )

set(ALL_SRCS
    ${StellarSolver_SRCS}
    ${sep_SRCS}
    ${engine_SRCS}
    ${anfiles_SRCS}
    ${anutils_SRCS}
    ${anbase_SRCS}
    ${kd_SRCS}
    ${qfits_SRCS}
    )

if (WIN32)
    add_library(stellarsolver STATIC ${ALL_SRCS})
else(WIN32)
    add_library(stellarsolver SHARED ${ALL_SRCS})
endif(WIN32)

target_link_libraries(stellarsolver
    ${CFITSIO_LIBRARIES}
    ${GSL_LIBRARIES}
    ${WCSLIB_LIBRARIES}
    Qt5::Core
    Qt5::Network
    Qt5::Widgets
    Qt5::Concurrent
    )

if(WIN32)
    target_link_libraries(stellarsolver wsock32 ${Boost_LIBRARIES})
else(WIN32)
    set_target_properties(stellarsolver PROPERTIES VERSION ${StellarSolver_VERSION_STRING} SOVERSION ${StellarSolver_SOVERSION} OUTPUT_NAME stellarsolver)
    target_link_libraries(stellarsolver -lpthread)
endif(WIN32)

if (MSVC) # We need to disable some warnings caused by using code designed for Unix on a Windows machine, otherwise astrometry code gives over 1500 warnings.
    # Use secure functions by default and suppress warnings about deprecated or POSIX functions
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D _CRT_NONSTDC_NO_DEPRECATE=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D _CRT_NONSTDC_NO_WARNINGS=1 /D _CRT_SECURE_NO_WARNINGS=1")
    # Disabling warnings caused by assigning values to types defined with typedef double <variable>
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4244")
    # Disbling warnings caused by size_t being assigned to int
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4267")
    # Disabling warning for comparing signed and unsigned variables
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4018")
    # Disabling warning for void function returning a value, since this is sometimes used in C programming and astrometry is C
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4098")
    # Disabling warning for unused local variables (This one is important because some variables only get used on Linux or MacOS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4101")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")
endif (MSVC)

install(TARGETS stellarsolver
    EXPORT ${TARGET_EXPORT_NAME}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/libstellarsolver
    )

set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include/libstellarsolver")
set(PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
set(PKGCONFIG_INSTALL_PREFIX "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/stellarsolver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/structuredefinitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/sextractorsolver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/parameters.h
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    DESTINATION "${INCLUDE_INSTALL_DIR}")
install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver/astrometry/include/astrometry
    DESTINATION "${INCLUDE_INSTALL_DIR}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/stellarsolver.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/stellarsolver.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/stellarsolver.pc DESTINATION ${PKGCONFIG_INSTALL_PREFIX})

#########################################################################################
## Stellar Solver Tester
#########################################################################################
if(BUILD_TESTER)

set(StellarSolverTester_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/tester/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tester/mainwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tester/imagelabel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tester/stretch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tester/dms.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tester/bayer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tester/resources.qrc
    )

qt5_wrap_ui(StellarSolverui_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/tester/mainwindow.ui
    )

if(APPLE)
set(MACOSX_BUNDLE_ICON_FILE StellarSolverIcon.icns)
set(StellarSolverTesterApp_ICON ${CMAKE_CURRENT_SOURCE_DIR}/tester/StellarSolverIcon.icns)
set_source_files_properties(${StellarSolverTesterApp_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif(APPLE)

if(WIN32)
set(StellarSolverTesterApp_ICON ${CMAKE_CURRENT_SOURCE_DIR}/tester/windowsicon.rc)
endif(WIN32)

add_executable(StellarSolverTester ${StellarSolverTester_SRCS} ${StellarSolverui_SRCS} ${StellarSolverTesterApp_ICON} ${ALL_SRCS})

if(APPLE)
    SET_TARGET_PROPERTIES(StellarSolverTester PROPERTIES MACOSX_BUNDLE TRUE)
endif(APPLE)


target_link_libraries(StellarSolverTester
    ${CFITSIO_LIBRARIES}
    ${GSL_LIBRARIES}
    ${WCSLIB_LIBRARIES}
    Qt5::Gui
    Qt5::Widgets
    Qt5::Core
    Qt5::Network
    Qt5::Concurrent
    )

if(WIN32)
    target_link_libraries(StellarSolverTester wsock32 ${Boost_LIBRARIES})
else(WIN32)
    target_link_libraries(StellarSolverTester -lpthread)
endif(WIN32)

if(APPLE)
install(TARGETS StellarSolverTester
    BUNDLE DESTINATION ${CMAKE_INSTALL_PREFIX}
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}
    )
else(APPLE)
        #installation for Linux
        install(TARGETS StellarSolverTester RUNTIME DESTINATION bin)
endif(APPLE)

endif(BUILD_TESTER)

#########################################################################################
# Generate Package Config Files
#########################################################################################
# Set destination directory
set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# 1. Generate the StellarSolverConfigVersion.cmake
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_EXPORT_CONFIG}Version.cmake"
    VERSION ${${PROJECT_NAME}_VERSION}
    COMPATIBILITY SameMajorVersion
    )

# 2. Configure the Exports and namespace
export(TARGETS stellarsolver
    NAMESPACE ${PROJECT_NAME}::
    FILE ${TARGET_EXPORT_NAME}.cmake)
# 3. Configure the Include Directories
target_include_directories(stellarsolver PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/libstellarsolver>)

# 4. Install StellarSolver Config
install(EXPORT ${TARGET_EXPORT_NAME}
        FILE ${TARGET_EXPORT_CONFIG}.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${ConfigPackageLocation})

# 5. Install StellarSolver Config Version
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_EXPORT_CONFIG}Version.cmake"
        DESTINATION ${ConfigPackageLocation})
