# - Try to find NFFT.
# Usage: find_package(NFFT [COMPONENTS [double threads]])
#
# Variables used by this module:
#  NFFT_ROOT_DIR             - NFFT root directory
# Variables defined by this module:
#  NFFT_FOUND                - system has NFFT
#  NFFT_INCLUDE_DIR          - the NFFT include directory
#  NFFT_INCLUDE_DIRS         - the NFFT include directories
#                              (identical to NFFT_INCLUDE_DIR)
#  NFFT_LIBRARY              - the NFFT library - double precision
#  NFFT_THREADS_LIBRARY      - the threaded NFFT library - double precision
#  NFFT_LIBRARIES            - list of all NFFT libraries found

# Copyright (C) 2014 Ghislain Vaillant <ghisvail@gmail.com>
#
# This file is heavily inspired from the FindFFTW3.cmake module developped by 
# ASTRON for their LOFAR software suite.

# Use double precision by default.
if(NFFT_FIND_COMPONENTS MATCHES "^$")
  set(_components double)
else()
  set(_components ${NFFT_FIND_COMPONENTS})
endif()

# Loop over each component.
set(_libraries)
foreach(_comp ${_components})
  if(_comp STREQUAL "double")
    list(APPEND _libraries nfft3)
  elseif(_comp STREQUAL "threads")
    set(_use_threads ON)
  else(_comp STREQUAL "double")
    message(FATAL_ERROR "FindNFFT: unknown component `${_comp}' specified. "
      "Valid components are `double' and `threads'.")
  endif(_comp STREQUAL "double")
endforeach(_comp ${_components})

# If using threads, we need to link against threaded libraries as well.
if(_use_threads)
  set(_thread_libs)
  foreach(_lib ${_libraries})
    list(APPEND _thread_libs ${_lib}_threads)
  endforeach(_lib ${_libraries})
  set(_libraries ${_thread_libs} ${_libraries})
endif(_use_threads)

# Keep a list of variable names that we need to pass on to
# find_package_handle_standard_args().
set(_check_list)

# Search for all requested libraries.
foreach(_lib ${_libraries})
  string(TOUPPER ${_lib} _LIB)
  find_library(${_LIB}_LIBRARY ${_lib}
    HINTS ${NFFT_ROOT_DIR} PATH_SUFFIXES lib)
  mark_as_advanced(${_LIB}_LIBRARY)
  list(APPEND NFFT_LIBRARIES ${${_LIB}_LIBRARY})
  list(APPEND _check_list ${_LIB}_LIBRARY)
endforeach(_lib ${_libraries})

# Search for the header file.
find_path(NFFT_INCLUDE_DIR nfft3.h 
  HINTS ${NFFT_ROOT_DIR} PATH_SUFFIXES include)
mark_as_advanced(NFFT_INCLUDE_DIR)
list(APPEND _check_list NFFT_INCLUDE_DIR)

# Handle the QUIETLY and REQUIRED arguments and set FFTW_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NFFT DEFAULT_MSG ${_check_list})
