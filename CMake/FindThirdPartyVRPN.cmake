# - try to find VRPN library
#
# Cache Variables:
#  VRPN_LIBRARY
#  VRPN_SERVER_LIBRARY
#  VRPN_INCLUDE_DIR
#
# Non-cache variables you might use in your CMakeLists.txt:
#  VRPN_FOUND
#  VRPN_SERVER_LIBRARIES - server libraries
#  VRPN_LIBRARIES - client libraries
#  VRPN_CLIENT_DEFINITIONS - definitions if you only use the client library
#  VRPN_DEFINITIONS - Client-only definition if all we found was the client library.
#  VRPN_INCLUDE_DIRS
#
# VRPN_ROOT_DIR is searched preferentially for these files
#
# Requires these CMake modules:
#  FindPackageHandleStandardArgs (known included with CMake >=2.6.2)
#
# Original Author:
# 2009-2012 Ryan Pavlik <rpavlik@iastate.edu> <abiryan@ryand.net>
# http://academic.cleardefinition.com
# Iowa State University HCI Graduate Program/VRAC
#
# Copyright Iowa State University 2009-2012.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

set(VRPN_ROOT_DIR
	"${VRPN_ROOT_DIR}"
	CACHE
	PATH
	"Root directory to search for VRPN")

if("${CMAKE_SIZEOF_VOID_P}" MATCHES "8")
	set(_libsuffixes lib64 lib)

	# 64-bit dir: only set on win64
	file(TO_CMAKE_PATH "$ENV{ProgramW6432}" _progfiles)
else()
	set(_libsuffixes lib)
	set(_PF86 "ProgramFiles(x86)")
	if(NOT "$ENV{${_PF86}}" STREQUAL "")
		# 32-bit dir: only set on win64
		file(TO_CMAKE_PATH "$ENV{${_PF86}}" _progfiles)
	else()
		# 32-bit dir on win32, useless to us on win64
		file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _progfiles)
	endif()
endif()

set(_vrpn_quiet)
if(VRPN_FIND_QUIETLY)
	set(_vrpn_quiet QUIET)
endif()

###
# Configure VRPN
###

find_path(VRPN_INCLUDE_DIR
	NAMES
	vrpn_Connection.h
	PATH_SUFFIXES
	include
	include/vrpn
	HINTS
	"${VRPN_ROOT_DIR}"
	PATHS
	"${_progfiles}/VRPN"
	C:/usr/local
	/usr/local)

find_library(VRPN_LIBRARY
	NAMES
	vrpn
	PATH_SUFFIXES
	${_libsuffixes}
	HINTS
	"${VRPN_ROOT_DIR}"
	PATHS
	"${_progfiles}/VRPN"
	C:/usr/local
	/usr/local)

find_library(VRPN_SERVER_LIBRARY
	NAMES
	vrpnserver
	PATH_SUFFIXES
	${_libsuffixes}
	HINTS
	"${VRPN_ROOT_DIR}"
	PATHS
	"${_progfiles}/VRPN"
	C:/usr/local
	/usr/local)

###
# Dependencies
###
set(_deps_libs)
set(_deps_includes)
set(_deps_check)

find_package(quatlib ${_vrpn_quiet})
list(APPEND _deps_libs ${QUATLIB_LIBRARIES})
list(APPEND _deps_includes ${QUATLIB_INCLUDE_DIRS})
list(APPEND _deps_check QUATLIB_FOUND)

if(NOT WIN32)
	find_package(Threads ${_vrpn_quiet})
	list(APPEND _deps_libs ${CMAKE_THREAD_LIBS_INIT})
	list(APPEND _deps_check Threads_FOUND)
endif()

if(WIN32)
	find_package(Libusb1 QUIET)
	if(LIBUSB1_FOUND)
		list(APPEND _deps_libs ${LIBUSB1_LIBRARIES})
		list(APPEND _deps_includes ${LIBUSB1_INCLUDE_DIRS})
	endif()
endif()


# handle the QUIETLY and REQUIRED arguments and set xxx_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VRPN
	DEFAULT_MSG
	VRPN_LIBRARY
	VRPN_INCLUDE_DIR
	${_deps_check})

if(VRPN_FOUND)
	set(VRPN_INCLUDE_DIRS "${VRPN_INCLUDE_DIR}" ${_deps_includes})
	set(VRPN_LIBRARIES "${VRPN_LIBRARY}" ${_deps_libs})
	set(VRPN_SERVER_LIBRARIES "${VRPN_SERVER_LIBRARY}" ${_deps_libs})

	if(VRPN_LIBRARY)
		set(VRPN_CLIENT_DEFINITIONS -DVRPN_CLIENT_ONLY)
	else()
		unset(VRPN_CLIENT_DEFINITIONS)
	endif()

	if(VRPN_LIBRARY AND NOT VRPN_SERVER_LIBRARY)
		set(VRPN_DEFINITIONS -DVRPN_CLIENT_ONLY)
	else()
		unset(VRPN_DEFINITIONS)
	endif()

	mark_as_advanced(VRPN_ROOT_DIR)
endif()

mark_as_advanced(VRPN_LIBRARY VRPN_SERVER_LIBRARY VRPN_INCLUDE_DIR)



###############################################################################
# Software License Agreement (AGPL-3 License)
#
# Module to find VRPN library and create a target to link against.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License version 3,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.
# If not, see <http://www.gnu.org/licenses/>.
###############################################################################

#get_property(OV_PRINTED GLOBAL PROPERTY OV_TRIED_ThirdPartyVRPN)

#if(EXISTS ${LIST_DEPENDENCIES_PATH}/vrpn)
#    set(VRPN_DIR ${LIST_DEPENDENCIES_PATH}/vrpn)
#endif()

#find_library(VRPN_LIBRARY NAMES vrpn vrpnd PATHS ${VRPN_DIR} PATH_SUFFIXES lib)
#find_library(VRPN_SERVER_LIBRARY NAMES vrpnserver PATHS ${VRPN_DIR} PATH_SUFFIXES lib)
#find_library(QUAT_LIBRARY NAMES quat quatd PATHS ${VRPN_DIR} PATH_SUFFIXES lib)

#if(NOT VRPN_LIBRARY STREQUAL VRPN_LIBRARY-NOTFOUND
#   AND NOT QUAT_LIBRARY STREQUAL QUAT_LIBRARY-NOTFOUND
#   AND NOT VRPN_SERVER_LIBRAR STREQUAL VRPN_SERVER_LIBRARY-NOTFOUND)

#    ov_print(OV_PRINTED "Found VRPN library")

    # Create target to link against.
#    add_library(vrpn INTERFACE)
#    target_include_directories(vrpn INTERFACE ${VRPN_DIR}/include)
#    set(VRPN_INCLUDE_DIR aaaaa${VRPN_DIR}/include)
#    target_link_libraries(vrpn INTERFACE ${VRPN_LIBRARY} ${VRPN_SERVER_LIBRARY} ${QUAT_LIBRARY})

#else()
    # Add empty target to avoid errors in CMakeLists linking against it
#    add_library(vrpn INTERFACE)
#    ov_print(OV_PRINTED "  FAILED to find VRPN")
#endif()

#set_property(GLOBAL PROPERTY OV_TRIED_ThirdPartyVRPN "Yes")