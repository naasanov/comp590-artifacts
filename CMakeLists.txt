cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0026 OLD)
cmake_policy(SET CMP0039 NEW)
cmake_policy(SET CMP0042 OLD)
cmake_policy(SET CMP0043 OLD)
cmake_policy(SET CMP0045 OLD)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0057 NEW)

set(OPENVIBE_MAJOR_VERSION 3)
set(OPENVIBE_MINOR_VERSION 5)
set(OPENVIBE_PATCH_VERSION 0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(OpenVIBE VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

set(CMAKE_MODULE_PATH_BASE "${CMAKE_SOURCE_DIR}/CMake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH_BASE}
                      ${CMAKE_MODULE_PATH_BASE}/drivers
)


## ###################################################################
## Output directory setup
## ###################################################################

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
list(APPEND CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)

if("${isSystemDir}" STREQUAL "-1")
   set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif("${isSystemDir}" STREQUAL "-1")


include("Utilities")
include("OvPrint")
include("OvInstallLaunchScript")

set(BUILD_ARCH "x64" CACHE STRING "Architecture tu build for (x64 or x86)")
if(${BUILD_ARCH} STREQUAL "x64")
  set(DEPENDENCIES_ARCH "_x64")
endif()
if (UNIX)  #TODO: install deps in _x64 foler on unix and remove this condition
  set(DEPENDENCIES_ARCH "")
endif()

set(CMAKE_INSTALL_LIBDIR lib)  # Enforce as all distros may not have the same default (e.g. lib, lib32/lib64)
# Set CMake variables
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/../dist/${BUILD_ARCH}" CACHE PATH "Install directory" FORCE)
endif()

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
  string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIGU )
  string(CONCAT DIST_ROOT ${DIST_ROOT} $<$<CONFIG:${OUTPUTCONFIGU}>:${CMAKE_INSTALL_PREFIX}/${OUTPUTCONFIG}>)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIGU} "${CMAKE_INSTALL_PREFIX}/${OUTPUTCONFIG}/bin" )
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIGU} "${CMAKE_INSTALL_PREFIX}/${OUTPUTCONFIG}/lib" )
endforeach()

set(DIST_ROOT ${CMAKE_INSTALL_PREFIX})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(BUILD_DATADIR ${CMAKE_BINARY_DIR}/share/openvibe)

set(DIST_BINDIR ${DIST_ROOT}/bin)
set(DIST_LIBDIR ${DIST_ROOT}/lib)
set(DIST_INCLUDEDIR ${DIST_ROOT}/include)
set(DIST_DATADIR ${DIST_ROOT}/share)
set(DIST_SYSCONFDIR ${DIST_ROOT}/etc)
set(DIST_BINDIR_ABSOLUTE ${DIST_BINDIR})

# Add directories that OpenViBE will use to look for its components runtime, unless overridden by environment variables in the launch scripts.
# These defines should only be used in "openvibe/ov_directories.h".
add_definitions(-DOV_CMAKE_PATH_ROOT="${DIST_ROOT}")
add_definitions(-DOV_CMAKE_PATH_BIN="${DIST_ROOT}/${CMAKE_INSTALL_BINDIR}")
add_definitions(-DOV_CMAKE_PATH_LIB="${DIST_ROOT}/${CMAKE_INSTALL_LIBDIR}")
add_definitions(-DOV_CMAKE_PATH_DATA="${DIST_ROOT}/share/openvibe")

# Set OV variables
set(LIST_DEPENDENCIES_PATH "${CMAKE_SOURCE_DIR}/dependencies${DEPENDENCIES_ARCH}" CACHE PATH "Dependencies folder" )
# Create a path usable by Unix shell scripts (used by .sh launchers)
set(OV_PATH_DEPENCENCY_LIBS "")
foreach(TMP ${LIST_DEPENDENCIES_PATH})
	list(APPEND OV_PATH_DEPENDENCY_LIBS "${TMP}/lib")
endforeach()
string(REPLACE ";" ":" OV_PATH_PATH_DEPENDENCY_LIBS "${OV_PATH_DEPENDENCY_LIBS}")

set(OV_CONFIG_SUBDIR "${PROJECT_NAME}-${PROJECT_VERSION}" CACHE STRING "Subdirectory under user directory when configuration and logs will be saved")

set(OVT_TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/test-data")


# Options
option(Flag_VerboseOutput "Verbose CMake output" OFF)
option(OV_DISPLAY_ERROR_LOCATION "display complete error locations" ON)
option(OV_PACKAGE "Create a package in build directory" OFF)
option(BUILD_VALIDATION_TEST "Build the validation tests" ON)
option(BUILD_UNIT_TEST "Build the unit tests" ON)
option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" OFF)

message(STATUS "META OPTIONS : ")
message(STATUS "  Flag_VerboseOutput = '${Flag_VerboseOutput}'")
message(STATUS "  OV_DISPLAY_ERROR_LOCATION = '${OV_DISPLAY_ERROR_LOCATION}'")
message(STATUS "  OV_PACKAGE = '${OV_PACKAGE}'")
message(STATUS "  BUILD_VALIDATION_TEST = '${BUILD_VALIDATION_TEST}'")
message(STATUS "  BUILD_UNIT_TEST = '${BUILD_UNIT_TEST}'")
message(STATUS "  BUILD_DOCUMENTATION = '${BUILD_DOCUMENTATION}'")

# Add some dependencies to PATH
#Could (SHOULD) be replaced with find_path / find_package functionnalities
set(ENV{PATH} "$ENV{PATH};${LIST_DEPENDENCIES_PATH}/expat/bin")
set(ENV{PATH} "$ENV{PATH};${LIST_DEPENDENCIES_PATH}/vcredist")
set(ENV{PATH} "$ENV{PATH};${LIST_DEPENDENCIES_PATH}/gtk/bin")

# By setting SKIP[_FOLDER]* you can skip a subtree (example: SKIP_A_B_C skips folder a/b/c and all its subfolders if any)
# Skip building documentation
# a list of all project which will be filled by the directory traversal. This is needed to generate the documentation.
set_property(GLOBAL PROPERTY OV_PROP_CURRENT_PROJECTS "")
set_property(GLOBAL PROPERTY OV_PROP_CURRENT_PROJECTS_BUILD_DIR "")

# Sort target into directories for better visualization in IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(APP_FOLDER Applications)
set(KERNEL_FOLDER Kernel)
set(MODULES_FOLDER Modules)
set(PLUGINS_FOLDER Plugins)
set(MISC_FOLDER Misc)
set(TESTS_FOLDER Unit-Tests)
set(VALIDATION_FOLDER Validation-Tests)

# #############################################################################
# Find external projects
include("FindThirdPartyPython")
include("FindThirdPartyBoost")
include("FindThirdPartyEigen")
include("FindThirdPartyExpat")
include("FindThirdPartyFFTW3")
include("FindThirdPartyGTest")
include("FindThirdPartyGTK2")
include("FindThirdPartyITPP")
include("FindThirdPartyLSL")
include("FindThirdPartyLua")
include("FindThirdPartyOpenAL")
include("FindThirdPartyTinyXML2")
include("FindThirdPartyVRPN")
include("FindThirdPartyZLib")
include("FindThirdPartyOpenGL")
include("FindThirdPartyXercesC")
#find_package(Qt6 REQUIRED COMPONENTS Core Qml Gui Quick QuickControls2 Test)

# #############################################################################
# install conda env libraries for execution
# excluding a few that are not needed at runtime and collide with GTK dlls
if (WIN32)
	install(DIRECTORY $ENV{CONDA_PREFIX}/Library/bin/ 
			DESTINATION ${DIST_BINDIR} 
			FILES_MATCHING PATTERN "*.dll"
      PATTERN "intl.dll" EXCLUDE
      PATTERN "intl-8.dll" EXCLUDE
      PATTERN "iconv.dll" EXCLUDE
  )
endif()


if(QT_KNOWN_POLICY_QTP0001)
  qt_policy(SET QTP0001 NEW)
endif()

# #############################################################################
# Find Acquisition Drivers
include("FindThirdPartyBrainProductsAmplifierAPI")
include("FindThirdPartyEemagineEEGO")

# #############################################################################
# Add OpenViBE subdirectories

# Add SDK to build
add_subdirectory(sdk)

# Add Designer to build
add_subdirectory(designer)

# Add extras to build
add_subdirectory(extras)

# Add Documentation to Build
if(BUILD_DOCUMENTATION)
	add_subdirectory(documentation)
endif()
