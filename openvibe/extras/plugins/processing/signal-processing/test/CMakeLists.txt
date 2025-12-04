
project(test_eigen VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

if(WIN32)
	add_definitions(-DTARGET_OS_Windows)
endif(WIN32)
if(UNIX)
	add_definitions(-DTARGET_OS_Linux)
endif(UNIX)
add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
add_definitions(-DTARGET_ARCHITECTURE_i386)
	
include_directories(../src)

add_executable(${PROJECT_NAME} test_eigen.cpp)

target_link_libraries(${PROJECT_NAME}
					  openvibe
					  openvibe-module-system
					  Eigen3::Eigen
)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER ${TESTS_FOLDER})	# Place project in folder unit-test (for some IDE)

if(WIN32)	
	# Since cert, getting timeBeginPeriod() linker issues without this
	target_link_libraries(${PROJECT_NAME} winmm)
endif(WIN32)
