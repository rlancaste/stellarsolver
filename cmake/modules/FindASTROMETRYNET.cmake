# - Try to find astrometry.net
# Once done this will define
#
#  ASTROMETRYNET_FOUND - system has ASTROMETRYNET
#  ASTROMETRYNET_EXECUTABLE - the primary astrometry.net executable

# Copyright (c) 2016, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


if (ASTROMETRYNET_INCLUDE_DIR AND ASTROMETRYNET_LIBRARIES)

  # in cache already
  set(ASTROMETRYNET_FOUND TRUE)
  message(STATUS "Found ASTROMETRYNET: ${ASTROMETRYNET_LIBRARIES}")


else (ASTROMETRYNET_INCLUDE_DIR AND ASTROMETRYNET_LIBRARIES)

  if (NOT WIN32)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(PC_ASTROMETRYNET libastrometry)
    endif (PKG_CONFIG_FOUND)
  endif (NOT WIN32)

  find_path(ASTROMETRYNET_INCLUDE_DIR solver.h
      ${PC_CFITSIO_INCLUDE_DIRS}
      ${_obIncDir}
      ${GNUWIN32_DIR}/include
      $ENV{HOME}/AstroRoot/craft-root/include/astrometry
      /usr/include/astrometry
  )

  find_library(ASTROMETRYNET_LIBRARIES NAMES libastrometry.so libastrometry-dev libastrometry0
    PATHS
        ${PC_ASTROMETRYNET_LIBRARY_DIRS}
        ${_obIncDir}
        ${GNUWIN32_DIR}/include
        $ENV{HOME}/AstroRoot/craft-root/lib
        /usr/lib/x86_64-linux-gnu/lib
  )

  if(ASTROMETRYNET_INCLUDE_DIR AND ASTROMETRYNET_LIBRARIES)
    set(ASTROMETRYNET_FOUND TRUE)
  else (ASTROMETRYNET_INCLUDE_DIR AND ASTROMETRYNET_LIBRARIES)
    set(ASTROMETRYNET_FOUND FALSE)
  endif(ASTROMETRYNET_INCLUDE_DIR AND ASTROMETRYNET_LIBRARIES)

  if (ASTROMETRYNET_FOUND)
    if (NOT ASTROMETRYNET_FIND_QUIETLY)
      message(STATUS "Found ASTROMETRYNET: ${ASTROMETRYNET_LIBRARIES}")
    endif (NOT ASTROMETRYNET_FIND_QUIETLY)
  else (ASTROMETRYNET_FOUND)
    if (ASTROMETRYNET_FIND_REQUIRED)
      message(FATAL_ERROR "ASTROMETRYNET development library not found. Please install libastrometry-dev and try again.")
    endif (ASTROMETRYNET_FIND_REQUIRED)
  endif (ASTROMETRYNET_FOUND)

  mark_as_advanced(ASTROMETRYNET_INCLUDE_DIR ASTROMETRYNET_LIBRARIES)

endif (ASTROMETRYNET_INCLUDE_DIR AND ASTROMETRYNET_LIBRARIES)

