PROJECT(astrometry-sextractor C CXX)
cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_AUTOMOC ON)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")
include(GNUInstallDirs)
include_directories("$ENV{HOME}/AstroRoot/craft-root/include/astrometry")
include_directories("/usr/local/include")
include_directories("/usr/include")
#This one is for craft
include_directories("${CMAKE_INSTALL_PREFIX}/include")

set(CMAKE_CXX_FLAGS "-g -std=c++0x ${CMAKE_CXX_FLAGS}")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations" )
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-deprecated-declarations")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations" )
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wno-deprecated-declarations")

find_package(CFITSIO REQUIRED)
find_package(ASTROMETRYNET REQUIRED)
find_package(GSL REQUIRED)
find_package(Qt5 5.4 REQUIRED COMPONENTS Gui Widgets Core Concurrent)

if (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)
  message(FATAL_ERROR "CFITSIO version too old, Please install cfitsio 3.x and try again. http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html")
endif (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)

if (NOT ASTROMETRYNET_FOUND)
  message(FATAL_ERROR "Astrometry.net was not found on the system, please install astrometry.net")
endif (NOT ASTROMETRYNET_FOUND)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})

if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

if (ASTROMETRYNET_FOUND)
  include_directories(${ASTROMETRYNET_INCLUDE_DIR})
endif(ASTROMETRYNET_FOUND)

if (GSL_FOUND)
  include_directories(${GSL_INCLUDE_DIR})
endif (GSL_FOUND)

set(astrosex_SRCS

   ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/stretch.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/dms.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bayer.c

   #These are from astrometry.net, for some reason, they don't seem t be in the library?
   ${CMAKE_CURRENT_SOURCE_DIR}/resort-xylist.c
   ${CMAKE_CURRENT_SOURCE_DIR}/tabsort.c

   )

qt5_wrap_ui(astrosexui_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.ui
    )


add_executable(astrosex ${astrosex_SRCS} ${astrosexui_SRCS})

target_link_libraries(astrosex
    ${CFITSIO_LIBRARIES}
    ${GSL_LIBRARIES}
    ${ASTROMETRYNET_LIBRARIES}

    Qt5::Gui
    Qt5::Widgets
    Qt5::Core
    Qt5::Concurrent
    )


