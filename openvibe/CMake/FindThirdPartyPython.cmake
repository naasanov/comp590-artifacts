# ---------------------------------
# Finds third party python
# ---------------------------------

get_property(OV_PRINTED GLOBAL PROPERTY OV_TRIED_ThirdPartyPython)

set(Python3_FIND_VIRTUALENV FIRST)
if (WIN32)
  # needed when we run cmake in a conda environment
  set(Python3_FIND_REGISTRY LAST)
endif()
if (APPLE)
  # needed when we run cmake in a conda environment
  set(Python3_FIND_FRAMEWORK LAST)
endif()

find_package(Python3 3.10 COMPONENTS Interpreter Development)

if(Python3_FOUND)
  OV_PRINT(OV_PRINTED "  Found Python 3 at ${Python3_LIBRARIES}")
   ADD_DEFINITIONS(-DTARGET_HAS_ThirdPartyPython3)
else()
   OV_PRINT(OV_PRINTED "  FAILED to find Python 3 (needs v3.10 with bitness matching build target ${PLATFORM_TARGET})")
endif()

get_filename_component(PYTHON3_HOME ${Python3_EXECUTABLE} DIRECTORY)
target_compile_definitions(Python3::Python INTERFACE -DPYTHON_HOME="${PYTHON3_HOME}")

set_property(GLOBAL PROPERTY OV_TRIED_ThirdPartyPython "Yes")

