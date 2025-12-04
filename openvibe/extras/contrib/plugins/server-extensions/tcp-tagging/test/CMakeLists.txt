project(test_tagstream VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

if(WIN32)
        add_definitions(-DTARGET_OS_Windows)
endif(WIN32)
if(UNIX)
        add_definitions(-DTARGET_OS_Linux)
endif(UNIX)

include_directories(../)
add_executable(${PROJECT_NAME} test_tagstream.cpp ../ovasCTagStream.cpp)

target_link_libraries(${PROJECT_NAME}
                      openvibe
                      openvibe-module-system
                      Boost::boost
                      Boost::system
                      Boost::thread
)

if(UNIX AND NOT APPLE)
        find_library(LIB_RT rt)
        if(LIB_RT)
                target_link_libraries(${PROJECT_NAME} ${LIB_RT})
        else()
                message(WARNING "  FAILED to find rt...")
        endif()
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER ${TESTS_FOLDER})	# Place project in folder unit-test (for some IDE)
