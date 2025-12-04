project(OpenViBESDKTest VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

# ------------------
# Project variables
# ------------------

if(NOT DEFINED OVT_TEST_DATA_DIR)
	MESSAGE(FATAL_ERROR "Path to tests input data files is not set")
endif()

# ---------------------
# Project dependencies
# ---------------------

# Add some search directory to module path
set(OVT_CMAKE_DIR ${OV_BASE_DIR}/cmake-modules)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${OVT_CMAKE_DIR})

# --------------------
# Test configuration
# --------------------

# ${PROJECT_BINARY_DIR}/Testing/Temporary is automatically created by CTest for log files
# We create a temporary subdir for our test that will be create before each test is run
# and removed afterwards (see CTestCustom.cmake.in)
set(OVT_TEST_TEMPORARY_DIR ${OV_BASE_BIN_DIR}/Testing/Temporary/OVT/)

# Create test data directory for big or binary files
file(MAKE_DIRECTORY ${OVT_TEST_DATA_DIR})

# Ctest custom options
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CTestCustom.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake @ONLY)

# Memcheck configuration
IF(UNIX)
	find_program(MEMORYCHECK_COMMAND valgrind /usr/bin/valgrind)
	file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/data/resources/valgrind.supp" MEMORYCHECK_SUPPRESSIONS_FILE)
	set(MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full --run-libc-freeres=no --show-possibly-lost=no" )
ELSEIF(WIN32)
	find_program(MEMORYCHECK_COMMAND purify "C:/Program Files (x86)/")
ENDIF()

# Timeout must be set first
set (DART_TESTING_TIMEOUT 72000 CACHE STRING "Maximum time allowed before CTest will kill the test." FORCE)
include(CTest)
enable_testing()

# Utility variable for subproject
set(OVT_OPENVIBE_DATA ${CMAKE_CURRENT_BINARY_DIR}/data/openvibe)

# ----------------------
# CMake tree processing
# ----------------------

# Process tools first.
# Some important variables are defined within tools projects for their use by other
# projects as:
# - OVT_UNIT_TOOLKIT_INCLUDE_DIR
# - OVT_UNIT_TOOLKIT_TARGET_FILE
# - OVT_ACCURACY_TARGET_FILE
add_subdirectory(unit-toolkit)

include_directories(${OVT_UNIT_TOOLKIT_INCLUDE_DIR})

# Process unit tests
# When adding a new test driver, driver directory must be added here
add_subdirectory(ov-base)
add_subdirectory(openvibe-module-csv)
add_subdirectory(openvibe-module-fs)
add_subdirectory(openvibe-module-ebml)
add_subdirectory(openvibe-module-xml)
add_subdirectory(openvibe-module-socket)
add_subdirectory(openvibe-module-system)
add_subdirectory(openvibe-toolkit)
add_subdirectory(openvibe-plugin-stream-codecs)
