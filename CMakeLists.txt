PROJECT(astrometry-sextractor C CXX)
cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_AUTOMOC ON)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")
include(GNUInstallDirs)
include_directories("$ENV{HOME}/AstroRoot/craft-root/include")
include_directories("/usr/local/include")
include_directories("/usr/include")

set(CMAKE_CXX_FLAGS "-g -std=c++0x ${CMAKE_CXX_FLAGS}")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations" )
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-deprecated-declarations")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations" )
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wno-deprecated-declarations")

find_package(CFITSIO REQUIRED)
find_package(GSL REQUIRED)
find_package(Qt5 5.4 REQUIRED COMPONENTS Gui Widgets Core Concurrent)

if (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)
  message(FATAL_ERROR "CFITSIO version too old, Please install cfitsio 3.x and try again. http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html")
endif (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})

if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

if (GSL_FOUND)
  include_directories(${GSL_INCLUDE_DIR})
endif (GSL_FOUND)

set(astrosex_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stretch.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/dms.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bayer.c
   )

qt5_wrap_ui(astrosexui_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.ui
    )


add_executable(astrosex ${astrosex_SRCS} ${astrosexui_SRCS})

target_link_libraries(astrosex
    ${CFITSIO_LIBRARIES}
    ${GSL_LIBRARIES}

    #Astrometry Libraries, Not sure which of these will be needed yet
    #"$ENV{HOME}/AstroRoot/craft-root/lib/libkd.a"
    #"$ENV{HOME}/AstroRoot/craft-root/lib/libqfits.a"
    #"$ENV{HOME}/AstroRoot/craft-root/lib/libanbase.a"
    #"$ENV{HOME}/AstroRoot/craft-root/lib/libanfiles.a"
    #"$ENV{HOME}/AstroRoot/craft-root/lib/libanutils.a"
    #"$ENV{HOME}/AstroRoot/craft-root/lib/libastrometry.a"

    Qt5::Gui
    Qt5::Widgets
    Qt5::Core
    Qt5::Concurrent
    )


