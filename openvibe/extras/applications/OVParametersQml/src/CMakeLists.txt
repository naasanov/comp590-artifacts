project(OVParametersObject VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

add_library(${PROJECT_NAME} SHARED
parameterBoolObject.h
parameterBoolObject.cpp
parameterDoubleObject.h
parameterDoubleObject.cpp
parameterIntObject.h
parameterIntObject.cpp
parameterListStringObject.h
parameterListStringObject.cpp
parameterObject.h
parameterObject.cpp
parameterStringObject.h
parameterStringObject.cpp
parametersObject.h
)

target_include_directories(${PROJECT_NAME}
                           PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                            $<INSTALL_INTERFACE:include> #TODO check
                          )

target_link_libraries(${PROJECT_NAME} PUBLIC 
Qt6::Core
OVParameters)

# Install files
install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION ${DIST_BINDIR}
		LIBRARY DESTINATION ${DIST_LIBDIR}
		ARCHIVE DESTINATION ${DIST_LIBDIR})
